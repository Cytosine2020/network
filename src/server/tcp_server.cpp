#include "server/tcp_server.hpp"

#include "wire/ipv4.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
// todo
constexpr uint16_t WINDOW = 65535;
constexpr uint16_t IDENTIFIER = 0;


ssize_t TCPSender::send(Slice<uint8_t> data) {
    std::unique_lock<std::mutex> sender_guard{sender_lock};

    if (index_increase(buffer_end, 1) == buffer_start) {
        if (closed) { return 0; }

        std::unique_lock<std::mutex> guard{lock};

        while (index_increase(buffer_end, 1) == buffer_start) {
            if (closed) { return 0; }

            full.wait(guard);
        }
    }

    uint32_t start = buffer_start;
    start = start == 0 ? buffer.size() - 1 : start - 1;

    size_t size = 0;

    if (start > buffer_end) {
        size_t len = std::min(data.size() - size, start - buffer_end);
        buffer[Range{buffer_end}][Range{0, len}].copy_from_slice(data[Range{0, len}]);
        size += len;
    } else {
        size_t len = std::min(data.size() - size, buffer.size() - buffer_end);
        buffer[Range{buffer_end}][Range{0, len}].copy_from_slice(data[Range{0, len}]);
        size += len;
        if (size < buffer.size()) {
            len = std::min(static_cast<uint32_t>(data.size() - size), start);
            buffer[Range{0, len}].copy_from_slice(data[Range{size}][Range{0, len}]);
            size += len;
        }
    }

    buffer_end = index_increase(buffer_end, size);

    return size;
}

void TCPSender::take(MutSlice<uint8_t> data) {
    uint32_t end = buffer_end;

    if (buffer_start < end) {
        data.copy_from_slice(buffer[Range{buffer_start}][Range{0, data.size()}]);
    } else {
        size_t len = std::min(data.size(), buffer.size() - buffer_start);
        data[Range{0, len}].copy_from_slice(buffer[Range{buffer_start}][Range{0, len}]);
        data[Range{len}].copy_from_slice(buffer[Range{0, data.size() - len}]);
    }

    buffer_start = index_increase(buffer_start, data.size());
    frame_send += data.size();

    full.notify_one();
}

void TCPReceiver::accept(uint32_t seq, Slice<uint8_t> data) {
    uint32_t buffer_seq = seq - frame_receive;
    uint32_t buffer_offset = buffer_seq + buffer_end;
    uint32_t window = get_window();

    if (closing) {
        uint32_t remain = close_seq - frame_receive;
        window = std::min(window, remain);
    }

    if (buffer_seq < window) {
        uint32_t size = std::min(static_cast<uint32_t>(data.size()), window - buffer_seq);
        if (size == 0) { return; }

        if (buffer_offset + size <= buffer.size()) {
            buffer[Range{buffer_offset}][Range{0, size}].copy_from_slice(data[Range{0, size}]);
        } else {
            uint32_t cut = buffer.size() - buffer_offset;
            buffer[Range{buffer_offset}].copy_from_slice(data[Range{0, cut}]);
            buffer[Range{0, size - cut}].copy_from_slice(data[Range{cut, size}]);
        }

        Fragment new_fragment{seq, size};

        std::list<Fragment>::iterator a{fragments.end()};
        std::list<Fragment>::iterator b{fragments.end()};
        std::list<Fragment>::iterator ptr{fragments.begin()};

        for (; ptr != fragments.end(); ++ptr) {
            if (ptr->seq + ptr->length >= new_fragment.seq) {
                a = ptr;
                b = ptr;

                if (ptr->seq < new_fragment.seq) {
                    new_fragment.length += new_fragment.seq - ptr->seq;
                    new_fragment.seq = ptr->seq;
                }

                break;
            }
        }

        for (; ptr != fragments.end(); ++ptr) {
            if (ptr->seq > new_fragment.seq + new_fragment.length) {
                b = ptr;

                auto last = ptr;
                --last;

                if (last->seq + last->length > new_fragment.seq + new_fragment.length) {
                    new_fragment.length = last->seq + last->length - new_fragment.seq;
                }

                break;
            }
        }

        fragments.erase(a, b);

        if (new_fragment.seq == frame_receive) {
            frame_receive += new_fragment.length;
            buffer_end = index_increase(buffer_end, new_fragment.length);
            empty.notify_one();
        } else {
            fragments.insert(b, new_fragment);
        }
    }
}

void TCPReceiver::close(uint32_t seq) {
    close_seq = seq;
    closing = true;
    empty.notify_one();
}

ssize_t TCPReceiver::recv(MutSlice<uint8_t> data) {
    std::unique_lock<std::mutex> receiver_guard{receiver_lock};

    if (buffer_start == buffer_end) {
        if (closed) { return 0; }

        std::unique_lock<std::mutex> guard{lock};

        while (buffer_start == buffer_end) {
            if (closed) { return 0; }

            empty.wait(guard);
        }
    }

    uint32_t end = buffer_end;

    size_t size = 0;

    if (end > buffer_start) {
        size_t len = std::min(data.size() - size, end - buffer_start);
        data[Range{0, len}].copy_from_slice(buffer[Range{buffer_start}][Range{0, len}]);
        size += len;
    } else {
        size_t len = std::min(data.size() - size, buffer.size() - buffer_start);
        data[Range{0, len}].copy_from_slice(buffer[Range{buffer_start}][Range{0, len}]);
        size += len;
        if (size < buffer.size()) {
            len = std::min(static_cast<uint32_t>(data.size() - size), end);
            data[Range{size}][Range{0, len}].copy_from_slice(buffer[Range{0, len}]);
            size += len;
        }
    }

    buffer_start = index_increase(buffer_start, size);

    return size;
}

void *TCPServer::tcp_sender(void *args_) {
    auto *args = reinterpret_cast<TCPSenderArgs *>(args_);

    for (;;) {
        // todo: time out
        // todo: merge requests
        auto buffer = args->request_receiver.recv();

        switch (buffer->type) {
            case Request::Create: {
                auto &request = buffer->inner.create;
                auto *sender = request.sender;

                args->connection.emplace(sender->addr, sender);

                auto send = args->send_queue.send();
                TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                    args->addr.ip_addr, sender->addr.ip_addr,
                                    args->addr.port, sender->addr.port,
                                    sender->frame_send++, sender->frame_receive,
                                    false, false, false, false, true,
                                    false, false, true, false,
                                    WINDOW, 0);
            }
                break;
            case Request::FrameReceive: {
                auto &request = buffer->inner.frame_receive;
                auto *sender = args->connection.find(request.dest)->second;

                sender->ack_receive = request.ack_receive;
                sender->frame_receive = request.frame_receive;

                auto send = args->send_queue.send();
                TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                    args->addr.ip_addr, sender->addr.ip_addr,
                                    args->addr.port, sender->addr.port,
                                    sender->frame_send, sender->frame_receive,
                                    false, false, false, false, true,
                                    false, false, false, false,
                                    WINDOW, 0);
            }
                break;
            case Request::AckReceive: {
                auto &request = buffer->inner.ack_receive;
                auto *sender = args->connection.find(request.dest)->second;

                sender->ack_receive = request.ack_receive;
            }
                break;
            case Request::FrameSend: {
                auto &request = buffer->inner.frame_receive;
                auto *sender = args->connection.find(request.dest)->second;

                size_t size = TCPHeader::max_payload(args->mtu);
                size = std::min(size, sender->get_size());

                if (size > 0) {
                    auto send = args->send_queue.send();
                    auto tcp_buffer = TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                                          args->addr.ip_addr, sender->addr.ip_addr,
                                                          args->addr.port, sender->addr.port,
                                                          sender->frame_send, sender->frame_receive,
                                                          false, false, false, false, true,
                                                          false, false, false, false,
                                                          WINDOW, size);

                    sender->take(*tcp_buffer);
                }
            }
                break;
            case Request::Close: {
                auto &request = buffer->inner.close;
                auto *sender = args->connection.find(request.dest)->second;

                sender->ack_receive = request.ack_receive;
                sender->frame_receive = request.frame_receive;
                sender->closed = true;

                auto send = args->send_queue.send();
                TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                    args->addr.ip_addr, sender->addr.ip_addr,
                                    args->addr.port, sender->addr.port,
                                    sender->frame_send++, sender->frame_receive + 1,
                                    false, false, false, false, true,
                                    false, false, false, true,
                                    WINDOW, 0);
            }
                break;
            default:
                cs120_abort("unknown type!");
        }
    }
}

void *TCPServer::tcp_receiver(void *args_) {
    auto *args = reinterpret_cast<TCPReceiverArgs *>(args_);

    for (;;) {
        auto buffer = args->recv_queue->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*buffer)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto[tcp_header, tcp_option, tcp_data] = tcp_split(ip_data);
        if (tcp_header == nullptr || complement_checksum(*ip_header, ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        EndPoint dest{ip_header->get_src_ip(), tcp_header->get_src_port()};

        auto ptr = args->connection.find(dest);
        if (ptr == args->connection.end()) {
            if (tcp_header->get_flags() == TCPFlags::Sync) {
                uint32_t dest_seq = tcp_header->get_sequence() + 1;
                uint32_t src_seq = 0; // todo

                auto *send = new TCPSender{dest, src_seq, src_seq, dest_seq};
                auto *recv = new TCPReceiver{dest, src_seq, dest_seq};

                args->connection.emplace(dest, recv);

                { *args->request_sender.send() = Request{Request::Create, {.create = {send}}}; }
                {
                    *args->connect_sender.send() = TCPConnection{
                            dest, send, recv, args->request_sender
                    };
                }
            } else {
                // todo: reset tcp package other than syn when no connection is established
            }
        } else {
            auto &receiver = ptr->second;

            if (tcp_header->get_reset()) {
                // todo
            }

            if (tcp_header->get_sync()) {
                // todo
            }

            bool receive_ack = false;

            if (tcp_header->get_ack()) {
                uint32_t ack_num = tcp_header->get_ack_number();
//                if (ack_num > receiver->frame_send) {
//                    // todo: handle invalid ack
//                } else
                if (ack_num > receiver->ack_receive) {
                    receiver->ack_receive = tcp_header->get_ack_number();
                    receive_ack = true;
                }
            }

            if (!tcp_data.empty()) {
                receiver->accept(tcp_header->get_sequence(), tcp_data);
            }

            if (tcp_header->get_fin()) {
                // todo: handle repeated fin
                receiver->close(tcp_header->get_sequence() + tcp_data.size());
            }

            if (!receiver->closed && receiver->finished()) {
                receiver->closed = true;
                *args->request_sender.send() = Request{
                        Request::Close,
                        {.close = {dest, receiver->ack_receive, receiver->frame_receive}}
                };
            } else if (!tcp_data.empty()) {
                *args->request_sender.send() = Request{
                        Request::FrameReceive,
                        {.frame_receive = {dest, receiver->ack_receive, receiver->frame_receive}}
                };
            } else if (receive_ack) {
                *args->request_sender.send() = Request{
                        Request::AckReceive,
                        {.ack_receive = {dest, receiver->ack_receive}}
                };
            }
        }
    }
}


TCPServer::TCPServer(std::shared_ptr<BaseSocket> &device, size_t size, EndPoint addr) :
        sender{}, receiver{}, device{device}, addr{addr} {
    auto[send, recv] = device->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;
        (void) ip_data;

        if (ip_header->get_protocol() != IPV4Protocol::TCP ||
            ip_header->get_dest_ip() != addr.ip_addr) { return false; }

        auto[tcp_header, tcp_option, tcp_data] = tcp_split(ip_data);
        if (tcp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (tcp_header->get_dest_port() != addr.port) { return false; }

        return true;
    }, size);

    auto[request_send, request_recv] = MPSCQueue<Request>::channel(size);

    auto[connect_send, connect_recv] = MPSCQueue<TCPConnection>::channel(size);

    connect_receiver = std::move(connect_recv);

    auto *sender_args = new TCPSenderArgs{
            .mtu = device->get_mtu(),
            .addr = addr,
            .send_queue = send,
            .request_receiver = std::move(request_recv),
            .connection = {},
    };

    auto *receiver_args = new TCPReceiverArgs{
            .addr = addr,
            .recv_queue = std::move(recv),
            .send_queue = std::move(send),
            .request_sender = std::move(request_send),
            .connect_sender = std::move(connect_send),
            .connection = {},
    };

    pthread_create(&sender, nullptr, tcp_sender, sender_args);
    pthread_create(&receiver, nullptr, tcp_receiver, receiver_args);
}
}
