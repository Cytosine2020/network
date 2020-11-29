#include "server/nat_server.hpp"

#include "wire.hpp"


namespace cs120 {
void *NatServer::nat_lan_to_wan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    size_t lowest_free_port = NAT_PORTS_BASE;

    uint32_t sub_net_mask = inet_addr("255.255.255.0");
    uint32_t sub_net_addr = inet_addr("192.168.1.0");

    size_t wan_mtu = args->wan->get_mtu();

    for (;;) {
        auto receive = args->lan->recv();
        auto *ip_header = receive->buffer_cast<struct ip>();
        uint32_t src_ip = ip_get_saddr(*ip_header).s_addr;

        // drop if send to subnet
        if ((src_ip & sub_net_mask) == sub_net_addr) { continue; }

        auto send = args->wan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else if (get_ipv4_data_size(*receive) > wan_mtu) {
            cs120_warn("package truncated!");
        } else {
            uint16_t lan_port = get_src_port_from_ip_datagram(*receive);

            auto value = assemble_nat_table_field(src_ip, lan_port, 1);

            uint16_t wan_port = 0;

            auto table_ptr = args->nat_reverse_table.find(value);
            if (table_ptr == args->nat_reverse_table.end()) {
                if (lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
                    cs120_abort("nat ports used up!");
                }

                wan_port = lowest_free_port++;

                printf("port mapping add: %s/%d <-> %d\n",
                       inet_ntoa(in_addr{src_ip}), lan_port, wan_port);

                args->nat_table[wan_port].store(value, std::memory_order_seq_cst);
                args->nat_reverse_table.emplace(value, wan_port);
            } else {
                wan_port = table_ptr->second;
            }

            ip_header->ip_src = in_addr{args->addr};
            checksum_ip(*send);

            set_src_port_from_ip_datagram(*receive, wan_port);

            (*send)[Range{0, receive->size()}].copy_from_slice(*receive);
        }
    }
}

void *NatServer::nat_wan_to_lan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    size_t lan_mtu = args->lan->get_mtu();

    for (;;) {
        auto receive = args->wan->recv();
        auto *ip_header = receive->buffer_cast<struct ip>();

        uint16_t wan_port = get_dest_port_from_ip_datagram(*receive);

        auto value = args->nat_table[wan_port].load(std::memory_order_seq_cst);

        if (get_nat_table_extra(value) == 0) { continue; }

        auto send = args->lan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else if (get_ipv4_data_size(*receive) > lan_mtu) {
            cs120_warn("package truncated!");
        } else {
            auto lan_ip = get_nat_table_ip(value);
            auto lan_port = get_nat_table_port(value);

            ip_header->ip_dst = in_addr{lan_ip};
            checksum_ip(*send);

            set_dest_port_from_ip_datagram(*receive, lan_port);

            (*send)[Range{0, receive->size()}].copy_from_slice(*receive);
        }
    }
}

NatServer::NatServer(uint32_t addr, std::unique_ptr<BaseSocket> lan,
                     std::unique_ptr<BaseSocket> wan) :
        lan_to_wan{}, wan_to_lan{},
        lan{std::move(lan)}, wan{std::move(wan)},
        nat_table{NAT_PORTS_SIZE}, nat_reverse_table{}, addr{addr} {
    pthread_create(&lan_to_wan, nullptr, nat_lan_to_wan, this);
    pthread_create(&wan_to_lan, nullptr, nat_wan_to_lan, this);
}
}
