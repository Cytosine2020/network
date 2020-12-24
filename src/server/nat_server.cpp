#include "server/nat_server.hpp"

#include "wire/wire.hpp"


namespace cs120 {
NatServer::NatServer(uint32_t lan_addr, uint32_t wan_addr, std::shared_ptr<BaseSocket> lan,
                     std::shared_ptr<BaseSocket> wan, size_t size,
                     const Array<EndPoint> &map_addr) :
        lan_to_wan{}, wan_to_lan{}, lan{std::move(lan)}, wan{std::move(wan)},
        lan_sender{}, wan_sender{}, lan_receiver{}, wan_receiver{},
        nat_table{NAT_PORTS_SIZE}, nat_reverse_table{},
        lowest_free_port{NAT_PORTS_BASE}, wan_addr{wan_addr} {
    for (auto &end_point: map_addr) {
        if (lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
            cs120_abort("nat ports used up!");
        }

        uint16_t wan_port = lowest_free_port++;

        nat_table[wan_port - NAT_PORTS_BASE].store(end_point);
        nat_reverse_table.emplace(end_point, wan_port);

        printf("port mapping add: %s:%d <-> %d\n", inet_ntoa(in_addr{end_point.ip_addr}),
               end_point.port, wan_port);
    }

    uint32_t sub_net_mask = inet_addr("255.255.255.0");
    uint32_t sub_net_addr = inet_addr("192.168.1.0");

    auto[lan_send, lan_recv] = this->lan->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;
        (void) ip_data;

        if (ip_header->get_src_ip() == lan_addr ||
            (ip_header->get_dest_ip() & sub_net_mask) == sub_net_addr ||
            ip_header->get_time_to_live() == 0) { return false; }

        return true;
    }, size);

    auto[wan_send, wan_recv] = this->wan->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;
        (void) ip_data;

        if (ip_header->get_dest_ip() != wan_addr ||
            ip_header->get_time_to_live() == 0) { return false; }

        return true;
    }, size);

    lan_sender = std::move(lan_send);
    wan_sender = std::move(wan_send);
    lan_receiver = std::move(lan_recv);
    wan_receiver = std::move(wan_recv);

    pthread_create(&lan_to_wan, nullptr, nat_lan_to_wan, this);
    pthread_create(&wan_to_lan, nullptr, nat_wan_to_lan, this);
}

void NatServer::nat_lan_to_wan() {
    size_t wan_mtu = wan->get_mtu();

    for (;;) {
        auto receive = lan_receiver->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*receive)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        uint32_t src_ip = ip_header->get_src_ip();

        uint16_t lan_port;
        switch (ip_header->get_protocol()) {
            case IPV4Protocol::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
                    cs120_warn("invalid package!");
                    continue;
                }

                auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
                if (icmp_data.size() < sizeof(ICMPData)) { continue; }
                auto *recv_port = reinterpret_cast<struct ICMPData *>(icmp_data.begin());

                switch (icmp_header->get_type()) {
                    case ICMPType::EchoReply:
                        lan_port = recv_port->dest_port;
                        break;
                    case ICMPType::EchoRequest:
                        lan_port = recv_port->src_port;
                        break;
                    default:
                        continue;
                }
                break;
            }
            case IPV4Protocol::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                uint16_t checksum = complement_checksum(*ip_header, ip_data);
                if (udp_header == nullptr || !udp_header->check_checksum(checksum)) {
                    cs120_warn("invalid package!");
                    continue;
                }
                lan_port = udp_header->get_src_port();
                break;
            }
            case IPV4Protocol::TCP: {
                auto *tcp_header = ip_data.buffer_cast<TCPHeader>();
                if (tcp_header == nullptr || complement_checksum(ip_data) != 0) {
                    cs120_warn("invalid package!");
                    continue;
                }
                lan_port = tcp_header->get_src_port();
                break;
            }
            default:
                continue;
        }

        size_t ip_data_size = ip_header->get_total_length();
        if (ip_data_size > wan_mtu) {
            cs120_warn("package truncated!");
            continue;
        }

        EndPoint end_point{src_ip, lan_port};

        uint16_t wan_port = 0;

        auto table_ptr = nat_reverse_table.find(end_point);
        if (table_ptr == nat_reverse_table.end()) {
            if (lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
                cs120_abort("nat ports used up!");
            }

            wan_port = lowest_free_port++;

            printf("port mapping add: %s:%d <-> %d\n",
                   inet_ntoa(in_addr{src_ip}), lan_port, wan_port);

            size_t index = wan_port - NAT_PORTS_BASE;

            nat_table[index].store(end_point);
            nat_reverse_table.emplace(end_point, wan_port);
        } else {
            wan_port = table_ptr->second;
        }

        ip_header->set_time_to_live(ip_header->get_time_to_live() - 1);
        ip_header->set_src_ip(wan_addr);
        ip_header->set_checksum(0);
        ip_header->set_checksum(complement_checksum(ip_header->into_slice()));

        switch (ip_header->get_protocol()) {
            case IPV4Protocol::ICMP: {
                auto *icmp_header = reinterpret_cast<ICMPHeader *>(ip_data.begin());
                auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
                auto *recv_port = reinterpret_cast<struct ICMPData *>(icmp_data.begin());

                switch (icmp_header->get_type()) {
                    case ICMPType::EchoReply:
                        recv_port->dest_port = wan_port;
                        break;
                    case ICMPType::EchoRequest:
                        recv_port->src_port = wan_port;
                        break;
                    default:
                        continue;
                }

                icmp_header->set_checksum(0);
                icmp_header->set_checksum(complement_checksum(ip_data));
                break;
            }
            case IPV4Protocol::UDP: {
                auto *udp_header = reinterpret_cast<UDPHeader *>(ip_data.begin());
                udp_header->set_src_port(wan_port);
                udp_header->set_checksum(0);
                udp_header->set_checksum_enable(complement_checksum(*ip_header, ip_data));
                break;
            }
            case IPV4Protocol::TCP: {
                auto *tcp_header = reinterpret_cast<TCPHeader *>(ip_data.begin());
                tcp_header->set_src_port(wan_port);
                tcp_header->set_checksum(0);
                tcp_header->set_checksum(complement_checksum(ip_data));
                break;
            }
            default:
                cs120_unreachable("checked before!");
        }

        auto send = wan_sender.try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            (*send)[Range{0, ip_data_size}].copy_from_slice((*receive)[Range{0, ip_data_size}]);
        }
    }
}

void NatServer::nat_wan_to_lan() {
    size_t lan_mtu = lan->get_mtu();

    for (;;) {
        auto receive = wan_receiver->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*receive)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        uint16_t wan_port;
        switch (ip_header->get_protocol()) {
            case IPV4Protocol::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
                    cs120_warn("invalid package!");
                    continue;
                }

                auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
                if (icmp_data.size() < sizeof(ICMPData)) { continue; }
                auto *recv_port = reinterpret_cast<struct ICMPData *>(icmp_data.begin());

                switch (icmp_header->get_type()) {
                    case ICMPType::EchoReply:
                        wan_port = recv_port->src_port;
                        break;
                    case ICMPType::EchoRequest:
                        wan_port = recv_port->dest_port;
                        break;
                    default:
                        continue;
                }

                break;
            }
            case IPV4Protocol::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                uint16_t checksum = complement_checksum(*ip_header, ip_data);
                if (udp_header == nullptr || !udp_header->check_checksum(checksum)) {
                    cs120_warn("invalid package!");
                    continue;
                }
                wan_port = udp_header->get_dest_port();
                break;
            }
            case IPV4Protocol::TCP: {
                auto *tcp_header = ip_data.buffer_cast<TCPHeader>();
                if (tcp_header == nullptr || complement_checksum(ip_data) != 0) {
                    cs120_warn("invalid package!");
                    continue;
                }
                wan_port = tcp_header->get_dest_port();
                break;
            }
            default:
                continue;
        }

        size_t index = wan_port - NAT_PORTS_BASE;
        if (index >= NAT_PORTS_SIZE) { continue; }

        auto end_point = nat_table[index].load();
        if (end_point.empty()) { continue; }

        size_t ip_data_size = ip_header->get_total_length();
        if (ip_data_size > lan_mtu) {
            cs120_warn("package truncated!");
            continue;
        }

        ip_header->set_time_to_live(ip_header->get_time_to_live() - 1);
        ip_header->set_dest_ip(end_point.ip_addr);
        ip_header->set_checksum(0);
        ip_header->set_checksum(complement_checksum(ip_header->into_slice()));

        switch (ip_header->get_protocol()) {
            case IPV4Protocol::ICMP: {
                auto *icmp_header = reinterpret_cast<ICMPHeader *>(ip_data.begin());
                auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
                auto *recv_port = reinterpret_cast<struct ICMPData *>(icmp_data.begin());

                switch (icmp_header->get_type()) {
                    case ICMPType::EchoReply:
                        recv_port->src_port = end_point.port;
                        break;
                    case ICMPType::EchoRequest:
                        recv_port->dest_port = end_point.port;
                        break;
                    default:
                        continue;
                }

                icmp_header->set_checksum(0);
                icmp_header->set_checksum(complement_checksum(ip_data));
                break;
            }
            case IPV4Protocol::UDP: {
                auto *udp_header = reinterpret_cast<UDPHeader *>(ip_data.begin());
                udp_header->set_dest_port(end_point.port);
                udp_header->set_checksum(0);
                udp_header->set_checksum_enable(complement_checksum(*ip_header, ip_data));
                break;
            }
            case IPV4Protocol::TCP: {
                auto *tcp_header = reinterpret_cast<TCPHeader *>(ip_data.begin());
                tcp_header->set_dest_port(end_point.port);
                tcp_header->set_checksum(0);
                tcp_header->set_checksum(complement_checksum(ip_data));
                break;
            }
            default:
                cs120_unreachable("checked before!");;
        }

        auto send = lan_sender.try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            (*send)[Range{0, ip_data_size}].copy_from_slice((*receive)[Range{0, ip_data_size}]);
        }
    }
}
}
