#include "server/tcp_server.hpp"

#include "wire/ipv4.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
TCPServer::TCPServer(std::unique_ptr<BaseSocket> device, size_t size,
                     uint32_t src_ip, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port,
                     TCPState status) :
        device{std::move(device)}, send_queue{}, recv_queue{},
        src_ip{src_ip}, dest_ip{dest_ip}, src_port{src_port}, dest_port{dest_port},
        identifier{1}, status{status}, window{0}, src_seq{0}, dest_seq{0} {
    auto[send, recv] = this->device->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;
        (void) ip_data;

        if (ip_header->get_protocol() != IPV4Protocol::TCP ||
            ip_header->get_src_ip() != dest_ip ||
            ip_header->get_dest_ip() != src_ip) { return false; }

        auto[tcp_header, tcp_option, tcp_data] = tcp_split(ip_data);
        if (tcp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (tcp_header->get_src_port() != dest_port ||
            tcp_header->get_dest_port() != src_port) { return false; }

        return true;
    }, size);

    send_queue = std::move(send);
    recv_queue = std::move(recv);
}

TCPServer TCPServer::accept(std::unique_ptr<BaseSocket> device, size_t size,
                            uint32_t src_ip, uint32_t dest_ip,
                            uint16_t src_port, uint16_t dest_port) {
    auto server = TCPServer{std::move(device), size, src_ip, dest_ip,
                            src_port, dest_port, TCPState::Listen};

    server.accept();

    return server;
}

void TCPServer::accept() {
    for (;;) {
        auto buffer = recv_queue->recv();

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

        switch (status) {
            case TCPState::Listen:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Sync: {
                        dest_seq = tcp_header->get_sequence() + 1;
                        window = tcp_header->get_window();

                        auto send = send_queue.send();

                        TCPHeader::generate_sync_ack((*send)[Range{}], identifier++,
                                                     src_ip, dest_ip, src_port, dest_port,
                                                     src_seq++, dest_seq, false, false, false,
                                                     window, Slice<uint8_t>{});

                        status = TCPState::SyncReceived;
                    }
                        break;
                    case TCPFlags::SyncAck:
                    case TCPFlags::Ack:
                    case TCPFlags::Fin:
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::SyncReceived:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Ack:
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {

                            status = TCPState::Established;

                            return;
                        }
                        break;
                    case TCPFlags::Sync:
                    case TCPFlags::SyncAck:
                    case TCPFlags::Fin:
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::SyncSent:
            case TCPState::Established:
            case TCPState::FinWait1:
            case TCPState::FinWait2:
            case TCPState::CloseWait:
            case TCPState::Closing:
            case TCPState::LastAck:
            case TCPState::TimeWait:
            case TCPState::Closed:
                break;
            default:
                cs120_unreachable("unknown state!");
        }
    }
}

ssize_t TCPServer::send(cs120::Slice<uint8_t> data) {
    {
        auto buffer = send_queue.send();

        TCPHeader::generate_ack((*buffer)[Range{}], identifier++, src_ip, dest_ip,
                                src_port, dest_port, src_seq, dest_seq,
                                false, false, false, false, false,
                                window, data);

        src_seq += data.size();
    }

    for (;;) {
        auto buffer = recv_queue->recv();

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

        switch (status) {
            case TCPState::Established:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Ack:
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {

                            return tcp_data.size();
                        }
                        break;
                    case TCPFlags::Fin:
                    case TCPFlags::Sync:
                    case TCPFlags::SyncAck:
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::Closed:
                return 0;
            case TCPState::LastAck:
            case TCPState::Listen:
            case TCPState::SyncSent:
            case TCPState::SyncReceived:
            case TCPState::FinWait1:
            case TCPState::FinWait2:
            case TCPState::CloseWait:
            case TCPState::Closing:
            case TCPState::TimeWait:
                break;
            default:
                cs120_unreachable("unknown state!");
        }
    }
}

ssize_t TCPServer::recv(cs120::MutSlice<uint8_t> data) {
    for (;;) {
        auto buffer = recv_queue->recv();

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

        switch (status) {
            case TCPState::Established:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Ack:
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {
                            dest_seq += tcp_data.size();

                            data[Range{0, tcp_data.size()}].copy_from_slice(tcp_data);

                            auto send = send_queue.send();
                            TCPHeader::generate_ack((*send)[Range{}], identifier++,
                                                    src_ip, dest_ip, src_port, dest_port,
                                                    src_seq, dest_seq,
                                                    false, false, false, false, false,
                                                    window, Slice<uint8_t>{});

                            return tcp_data.size();
                        }
                        break;
                    case TCPFlags::Fin:
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {
                            dest_seq += 1;

//                            {
//                                auto send = send_queue.send();
//                                TCPHeader::generate_ack((*send)[Range{}], identifier++,
//                                                        src_ip, dest_ip, src_port, dest_port,
//                                                        src_seq, dest_seq,
//                                                        false, false, false, false, false,
//                                                        window, Slice<uint8_t>{});
//                            }

                            {
                                auto send = send_queue.send();
                                TCPHeader::generate_fin((*send)[Range{}], identifier++,
                                                        src_ip, dest_ip, src_port, dest_port,
                                                        src_seq++, dest_seq,
                                                        false, false, false, false, true, false,
                                                        window, Slice<uint8_t>{});
                            }

                            status = TCPState::LastAck;
                        }
                        break;
                    case TCPFlags::Sync:
                    case TCPFlags::SyncAck:
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::LastAck:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Ack:
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {
                            status = TCPState::Closed;
                            return 0;
                        }
                        break;
                    case TCPFlags::Sync:
                    case TCPFlags::SyncAck:
                    case TCPFlags::Fin:
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::Closed:
                return 0;
            case TCPState::Listen:
            case TCPState::SyncSent:
            case TCPState::SyncReceived:
            case TCPState::FinWait1:
            case TCPState::FinWait2:
            case TCPState::CloseWait:
            case TCPState::Closing:
            case TCPState::TimeWait:
                break;
            default:
                cs120_unreachable("unknown state!");
        }
    }
}

void TCPServer::close() {
    {
        auto send = send_queue.send();
        TCPHeader::generate_fin((*send)[Range{}], identifier++, src_ip, dest_ip,
                                src_port, dest_port, src_seq++, dest_seq,
                                false, false, false, false, true, false,
                                window, Slice<uint8_t>{});
    }

    status = TCPState::FinWait1;

    for (;;) {
        auto buffer = recv_queue->recv();
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

        switch (status) {
            case TCPState::FinWait1:
                switch (tcp_header->get_flags()) {
                    case TCPFlags::Ack: {
                        if (dest_seq == tcp_header->get_sequence() &&
                            src_seq == tcp_header->get_ack_number()) {
                            status = TCPState::FinWait2;
                        }
                    }
                        break;
                    case TCPFlags::Sync:
                    case TCPFlags::SyncAck:
                    case TCPFlags::Fin:
                        if (dest_seq == tcp_header->get_sequence()) {
                            dest_seq += 1;

                            {
                                auto send = send_queue.send();
                                TCPHeader::generate_ack((*send)[Range{}], identifier++,
                                                        src_ip, dest_ip, src_port, dest_port,
                                                        src_seq, dest_seq,
                                                        false, false, false, false, false,
                                                        window, Slice<uint8_t>{});
                            }

                            if (src_seq == tcp_header->get_ack_number()) {
                                // todo
                                return;
                            } else {
                                status = TCPState::Closing;
                            }
                        }
                    case TCPFlags::Reset:
                    case TCPFlags::Error:
                        break;
                    default:
                        cs120_unreachable("unknown state!");
                }
                break;
            case TCPState::FinWait2:
                break;
            case TCPState::Closing:
                if (dest_seq == tcp_header->get_sequence() &&
                    src_seq == tcp_header->get_ack_number()) {
                    // todo
                    return;
                }

                break;
            case TCPState::TimeWait:
                // todo
                break;
            case TCPState::Closed:
                return;
            case TCPState::Listen:
            case TCPState::SyncSent:
            case TCPState::SyncReceived:
            case TCPState::Established:
            case TCPState::CloseWait:
            case TCPState::LastAck:
                break;
            default:
                cs120_unreachable("unknown state!");
        }
    }
}
}
