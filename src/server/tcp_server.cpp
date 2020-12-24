#include "server/tcp_server.hpp"

#include "wire/ipv4.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
// todo
constexpr uint16_t WINDOW = 65535;
constexpr uint16_t IDENTIFIER = 0;

void TCPReceiver::accept(uint32_t seq, Slice<uint8_t> data) {
    uint32_t buffer_seq = seq - frame_receive;
    uint32_t buffer_offset = buffer_seq + buffer_end;
    if (buffer_seq < get_window()) {
        uint32_t size = std::min(data.size(), get_window() - buffer_seq);

//        printf("receive: %d, %ld, window: %ld\n", buffer_seq, data.size(), get_window());
//        printf("receive before: %ld, %ld\n", buffer_start, buffer_end);
//        for (auto &item: fragments) { printf("\t%d, %d\n", item.seq - frame_receive, item.length); }

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

//        printf("receive after: %ld, %ld\n", buffer_start, buffer_end);
//        for (auto &item: fragments) { printf("\t%d, %d\n", item.seq - frame_receive, item.length); }
    }
}

ssize_t TCPReceiver::recv(MutSlice<uint8_t> data) {
    std::unique_lock<std::mutex> receiver_guard{receiver_lock};

    if (buffer_start == buffer_end) {
        std::unique_lock<std::mutex> guard{lock};

        while (buffer_start == buffer_end) {
            empty.wait(guard);
        }
    }

    size_t size = 0;

    if (buffer_end > buffer_start) {
        size_t len = std::min(data.size() - size, buffer_end - buffer_start);
        data[Range{0, len}].copy_from_slice(buffer[Range{buffer_start}][Range{0, len}]);
        size += len;
    } else {
        size_t len = std::min(data.size() - size, buffer.size() - buffer_start);
        data[Range{0, len}].copy_from_slice(buffer[Range{buffer_start}][Range{0, len}]);
        size += len;
        if (size < buffer.size()) {
            len = std::min(data.size() - size, buffer_end);
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
        auto buffer = args->request_receiver.recv(); // todo: time out

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
                                    WINDOW, Slice<uint8_t>{});
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
                                    WINDOW, Slice<uint8_t>{});
            }
                break;
            case Request::AckReceive: {
                auto &request = buffer->inner.ack_receive;
                auto *sender = args->connection.find(request.dest)->second;

                sender->ack_receive = request.ack_receive;
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
                                    sender->frame_send++, sender->frame_receive,
                                    false, false, false, false, true,
                                    false, false, false, true,
                                    WINDOW, Slice<uint8_t>{});
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
                { *args->connect_sender.send() = TCPConnection{dest, send, recv}; }
            } else {
                // todo: reset tcp package other than syn when no connection is established
            }
        } else {
            auto &receiver = ptr->second;

            if (tcp_header->get_reset()) {
                // todo
            }

            if (receiver->closed) {
                // todo: handle closed situation
            }

            bool receive_ack = false;

            if (tcp_header->get_ack()) {
                uint32_t ack_num = tcp_header->get_ack_number();
//                if (ack_num > receiver->frame_send) {
//                    // todo: invalid ack
//                } else
                if (ack_num > receiver->ack_receive) {
                    receiver->ack_receive = tcp_header->get_ack_number();
                    receive_ack = true;
                }
            }

            if (tcp_header->get_sync()) {
                // todo
            }

            if (tcp_header->get_fin()) {
                // todo: handle fin with data
                // todo: handle out of order fin

                receiver->frame_receive += 1;

                receiver->closed = true;

                *args->request_sender.send() = Request{
                    Request::Close,
                    {.close = {dest, receiver->ack_receive, receiver->frame_receive}}
                };
            } else if (!tcp_data.empty()) {
                receiver->accept(tcp_header->get_sequence(), tcp_data);

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


TCPServer::TCPServer(std::unique_ptr<BaseSocket> device, size_t size, EndPoint addr) :
        sender{}, receiver{}, device{std::move(device)}, addr{addr} {
    auto[send, recv] = this->device->bind([=](auto ip_header, auto ip_option, auto ip_data) {
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
