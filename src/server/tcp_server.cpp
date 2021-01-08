#include "server/tcp_server.hpp"

#include <chrono>
#include <thread>
#include <vector>
#include <queue>

#include "wire/ipv4.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
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

void TCPSender::take(uint32_t offset, MutSlice<uint8_t> data) {
    uint32_t end = buffer_end;
    uint32_t start = index_increase(buffer_start, offset);

    if (buffer_start < end) {
        data.copy_from_slice(buffer[Range{start}][Range{0, data.size()}]);
    } else {
        size_t len = std::min(data.size(), buffer.size() - start);
        data[Range{0, len}].copy_from_slice(buffer[Range{start}][Range{0, len}]);
        data[Range{len}].copy_from_slice(buffer[Range{0, data.size() - len}]);
    }
}

void TCPSender::ack_update(uint32_t ack) {
    if (closed && ack == frame_send && ack == close_seq + 1) {
        ack_receive = ack;
        return;
    }

    if (ack_receive <= frame_send) {
        if (ack_receive < ack && ack <= frame_send) {
            buffer_start = index_increase(buffer_start, ack - ack_receive);
            ack_receive = ack;
            full.notify_all();
        }
    } else {
        if (ack_receive < ack || ack <= frame_send) {
            buffer_start = index_increase(buffer_start, ack - ack_receive);
            ack_receive = ack;
            full.notify_all();
        }
    }
}

void TCPReceiver::accept(uint32_t seq, Slice<uint8_t> data) {
    uint32_t buffer_seq = seq - frame_receive;
    uint32_t buffer_offset = buffer_seq + buffer_end;
    uint32_t window = get_window();

    if (closed) {
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

            if (frame_receive == close_seq) { ++frame_receive; }

            empty.notify_all();
        } else {
            fragments.insert(b, new_fragment);
        }
    }
}

void TCPReceiver::close(uint32_t seq) {
    close_seq = seq;
    if (frame_receive == close_seq) { ++frame_receive; }
    closed = true;
    empty.notify_all();
}

ssize_t TCPReceiver::recv(MutSlice<uint8_t> data) {
    std::unique_lock<std::mutex> receiver_guard{receiver_lock};

    if (buffer_start == buffer_end) {
        if (finished()) { return 0; }

        std::unique_lock<std::mutex> guard{lock};

        while (buffer_start == buffer_end) {
            if (finished()) { return 0; }

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

void *TCPClient::tcp_sender(void *args_) {
    using namespace std::literals;
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

    auto *args = reinterpret_cast<TCPSendArgs *>(args_);
    auto sender = args->connection;

    std::priority_queue<TimePoint, std::vector<TimePoint>, std::greater<>> timeout{};

    for (;;) {
        if (args->send_queue.is_closed()) { break; }

        MPSCQueue<Request>::ReceiverSlotGuard recv =
                timeout.empty() ? args->request_receiver.recv().unwrap() :
                args->request_receiver.recv_deadline(timeout.top()).unwrap();

        auto current = std::chrono::system_clock::now();

        // this is timeout
        if (recv.none()) {
            if (sender->finished()) { break; }

            sender->generate_data(args->send_queue);

            timeout.pop();
            timeout.push(current + 300ms);

            continue;
        }

        switch (recv->type) {
            case Request::FrameReceive: {
                auto &request = recv->inner.frame_receive;

                sender->ack_update(request.ack_receive);
                sender->remote_window = sender->remote_window;
                sender->frame_receive = request.frame_receive;
                sender->local_window = request.local_window;

                sender->generate_ack(args->send_queue);
            }
                break;
            case Request::AckReceive: {
                auto &request = recv->inner.ack_receive;

                sender->ack_update(request.ack_receive);
                sender->remote_window = sender->remote_window;
            }
                break;
            case Request::FrameSend: {
                if (timeout.empty()) {
                    sender->generate_data(args->send_queue);

                    timeout.push(current + 300ms);
                }
            }
                break;
            case Request::Close: {
                if (!sender->closed) {
                    sender->closed = true;
                    sender->close_seq = sender->ack_receive + sender->get_size();

                    if (timeout.empty()) {
                        sender->generate_fin(args->send_queue);

                        timeout.push(current + 300ms);
                    }
                }
            }
                break;
            default:
                cs120_abort("unknown type!");
        }
    }

    delete args;

    return nullptr;
}

void *TCPClient::tcp_receiver(void *args_) {
    using namespace std::literals;

    auto *args = reinterpret_cast<TCPRecvArgs *>(args_);
    auto receiver = args->connection;

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

        if (!tcp_header->check_flags()) {
            // todo
        }

        if (tcp_header->get_reset()) {
            // todo
        }

        if (tcp_header->get_sync()) {
            // todo
        }

        if (tcp_header->get_fin()) {
            receiver->close(tcp_header->get_sequence() + tcp_data.size());
        }

        if (tcp_header->get_ack()) {
            uint32_t ack_num = tcp_header->get_ack_number();
            if (ack_num > receiver->ack_receive) {
                receiver->ack_receive = tcp_header->get_ack_number();
            }
        }

        if (!tcp_data.empty()) {
            receiver->accept(tcp_header->get_sequence(), tcp_data);
        }

        uint32_t remote_window = tcp_header->get_window() << receiver->scale;
        uint32_t local_window = receiver->get_window();

        if (!tcp_data.empty() || tcp_header->get_fin()) {
            auto send = args->request_sender.send();
            if (send.none()) { break; }
            *send = Request{Request::FrameReceive, {.frame_receive = {
                    receiver->ack_receive, remote_window,
                    receiver->frame_receive, local_window,
            }}};
        } else if (tcp_header->get_ack()) {
            auto send = args->request_sender.send();
            if (send.none()) { break; }
            *send = Request{Request::AckReceive, {.ack_receive = {
                    receiver->ack_receive, remote_window,
            }}};
        }
    }

    delete args;

    return nullptr;
}


TCPClient::TCPClient(std::shared_ptr<BaseSocket> &device, size_t size,
                     EndPoint local, EndPoint remote) :
        send_thread{}, recv_thread{}, device{device},
        sender{nullptr}, receiver{nullptr}, request_sender{} {
    auto[send, recv] = device->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;
        (void) ip_data;

        if (ip_header->get_protocol() != IPV4Protocol::TCP ||
            ip_header->get_dest_ip() != local.ip_addr ||
            ip_header->get_src_ip() != remote.ip_addr) { return false; }

        auto[tcp_header, tcp_option, tcp_data] = tcp_split(ip_data);
        if (tcp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (tcp_header->get_dest_port() != local.port ||
            tcp_header->get_src_port() != remote.port) { return false; }

        return true;
    }, size);

    uint16_t local_mss = TCPHeader::max_payload(device->get_mtu());
    uint16_t remote_mss = 0;

    uint8_t local_scale = 0;
    uint8_t remote_scale = 0;

    uint32_t local_seq = 0;
    uint32_t remote_seq = 0; // todo

    SyncOption option{local_mss, local_scale};

    uint32_t local_window = TCPSender::BUFFER_SIZE - 1;
    uint32_t remote_window = 0;

    uint16_t window = std::min<size_t>(local_window >> local_scale,
                                       std::numeric_limits<uint16_t>::max());

    {
        auto buffer = send.send();
        TCPHeader::generate((*buffer)[Range{}], 0, IDENTIFIER, local.ip_addr, remote.ip_addr, 64,
                            local.port, remote.port, local_seq, 0,
                            false, false, false, false, false, false, false, true, false,
                            window, option.into_slice(), 0);
    }

    bool sync = false, ack = false;

    for (;;) {
        using namespace std::literals;

        auto buffer = recv->recv_timeout(300ms).unwrap();
        if (buffer.none()) {
            auto send_buffer = send.send();
            TCPHeader::generate((*send_buffer)[Range{}], 0, IDENTIFIER,
                                local.ip_addr, remote.ip_addr, 64,
                                local.port, remote.port, local_seq, 0,
                                false, false, false, false, false, false, false, true, false,
                                window, option.into_slice(), 0);

            continue;
        }

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

        remote_window = tcp_header->get_window() << remote_scale;

        if (!tcp_header->check_flags()) {
            // todo
        }

        if (tcp_header->get_fin()) {
            // todo
        }

        if (tcp_header->get_reset()) {
            // todo≤
        }

        if (tcp_header->get_ack()) {
            if (ack) {
                if (local_seq + 1 != tcp_header->get_ack_number()) {
                    // todo
                }
            } else {
                ack = true;
                local_seq = tcp_header->get_ack_number();
            }
        }

        if (tcp_header->get_sync()) {
            if (sync) {
                if (remote_seq != tcp_header->get_sequence() + 1) {
                    // todo
                }
            } else {
                sync = true;
                remote_seq = tcp_header->get_sequence() + 1;
            }

            auto send_buffer = send.send();
            TCPHeader::generate((*send_buffer)[Range{}], 0, IDENTIFIER,
                                local.ip_addr, remote.ip_addr, 64,
                                local.port, remote.port, local_seq, remote_seq,
                                false, false, false, false, true, false, false, false, false,
                                local_window, Slice<uint8_t>{}, 0);
        }

        TCPOptionIter iter{tcp_option};
        // todo
        for (auto item = iter.next(); !iter.is_end(item); item = iter.next()) {
            auto[op, data] = item;

            switch (op) {
                case TCPOption::End:
                    break;
                case TCPOption::NoOperation:
                    break;
                case TCPOption::MaximumSegmentSize:
                    remote_mss = (static_cast<uint16_t>(data[0]) << 8) |
                                 (static_cast<uint16_t>(data[1]) << 0);
                    break;
                case TCPOption::WindowScaleFactor:
                    remote_scale = data[0];
                    break;
                case TCPOption::SACKPermitted:
                    break;
                case TCPOption::SACK:
                    break;
                case TCPOption::Echo:
                    break;
                case TCPOption::EchoReply:
                    break;
                case TCPOption::Timestamp:
                    break;
                default:
                    cs120_warn("unknown tcp option type!");
            }
        }

        if (ack && sync) { break; }
    }

    sender = std::shared_ptr<TCPSender>(new TCPSender{
            local, remote, local_seq, remote_seq, local_mss, local_scale, remote_window
    });
    receiver = std::shared_ptr<TCPReceiver>(new TCPReceiver{
            local, remote, local_seq, remote_seq, remote_mss, remote_scale
    });

    auto[request_send, request_recv] = MPSCQueue<Request>::channel(size);

    request_sender = request_send;

    auto *send_args = new TCPSendArgs{
            .send_queue = std::move(send),
            .request_receiver = std::move(request_recv),
            .connection = sender,
    };

    auto *recv_args = new TCPRecvArgs{
            .recv_queue = std::move(recv),
            .request_sender = std::move(request_send),
            .connection = receiver,
    };

    pthread_create(&send_thread, nullptr, tcp_sender, send_args);
    pthread_create(&recv_thread, nullptr, tcp_receiver, recv_args);
}
}
