#include "server/nat_server.hpp"

#include "wire.hpp"


namespace cs120 {
void *NatServer::nat_lan_to_wan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    uint32_t sub_net_mask = inet_addr("255.255.255.0");
    uint32_t sub_net_addr = inet_addr("192.168.1.0");

    size_t wan_mtu = args->wan->get_mtu();

    for (;;) {
        auto receive = args->lan->recv();
        auto *ip_header = receive->buffer_cast<struct ip>();
        size_t ip_data_size = get_ipv4_total_size(*receive);

        uint32_t src_ip = ip_get_saddr(*ip_header);
        uint32_t dest_ip = ip_get_daddr(*ip_header);

        // drop if send to subnet
        if (src_ip == args->ip_addr) { continue; }
        if ((dest_ip & sub_net_mask) == sub_net_addr) { continue; }

        if (ip_data_size > wan_mtu) {
            cs120_warn("package truncated!");
        }

        auto send = args->wan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            uint16_t lan_port = get_src_port_from_ip_datagram(*receive);

            auto value = assemble_nat_table_field(src_ip, lan_port);

            uint16_t wan_port = 0;

            auto table_ptr = args->nat_reverse_table.find(value);
            if (table_ptr == args->nat_reverse_table.end()) {
                if (args->lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
                    cs120_abort("nat ports used up!");
                }

                wan_port = args->lowest_free_port++;

                printf("port mapping add: %s:%d <-> %d\n",
                       inet_ntoa(in_addr{src_ip}), lan_port, wan_port);

                size_t index = wan_port - NAT_PORTS_BASE;

                args->nat_table[index].store(value);
                args->nat_reverse_table.emplace(value, wan_port);
            } else {
                wan_port = table_ptr->second;
            }

            ip_header->ip_src = in_addr{args->ip_addr};
            checksum_ip(*receive);

            set_src_port_from_ip_datagram(*receive, wan_port);

            (*send)[Range{0, ip_data_size}].copy_from_slice((*receive)[Range{0, ip_data_size}]);
        }
    }
}

void *NatServer::nat_wan_to_lan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    size_t lan_mtu = args->lan->get_mtu();

    for (;;) {
        auto receive = args->wan->recv();
        auto *ip_header = receive->buffer_cast<struct ip>();
        size_t ip_data_size = get_ipv4_total_size(*receive);

        uint16_t wan_port = get_dest_port_from_ip_datagram(*receive);

        size_t index = wan_port - NAT_PORTS_BASE;
        if (index >= NAT_PORTS_SIZE) { continue; }

        auto value = args->nat_table[index].load();

        if (get_nat_table_extra(value) == 0) { continue; }

        if (ip_data_size > lan_mtu) {
            cs120_warn("package truncated!");
            continue;
        }

        auto send = args->lan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            auto lan_ip = get_nat_table_ip(value);
            auto lan_port = get_nat_table_port(value);

            ip_header->ip_dst = in_addr{lan_ip};
            checksum_ip(*receive);

            set_dest_port_from_ip_datagram(*receive, lan_port);

            (*send)[Range{0, ip_data_size}].copy_from_slice((*receive)[Range{0, ip_data_size}]);
        }
    }
}

NatServer::NatServer(uint32_t ip_addr, std::unique_ptr<BaseSocket> lan,
                     std::unique_ptr<BaseSocket> wan,
                     const Array<std::pair<uint32_t, uint16_t>> &map_addr) :
        lan_to_wan{}, wan_to_lan{}, lan{std::move(lan)}, wan{std::move(wan)},
        nat_table{NAT_PORTS_SIZE}, nat_reverse_table{},
        lowest_free_port{NAT_PORTS_BASE}, ip_addr{ip_addr} {
    for (auto &[src_ip, lan_port]: map_addr) {
        if (lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
            cs120_abort("nat ports used up!");
        }

        uint16_t wan_port = lowest_free_port++;
        uint64_t value = assemble_nat_table_field(src_ip, lan_port);

        nat_table[wan_port - NAT_PORTS_BASE].store(value);
        nat_reverse_table.emplace(value, wan_port);

        printf("port mapping add: %s:%d <-> %d\n", inet_ntoa(in_addr{src_ip}),
               lan_port, wan_port);
    }

    pthread_create(&lan_to_wan, nullptr, nat_lan_to_wan, this);
    pthread_create(&wan_to_lan, nullptr, nat_wan_to_lan, this);
}
}
