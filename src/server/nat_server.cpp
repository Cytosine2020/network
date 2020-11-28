#include "server/nat_server.hpp"


namespace cs120 {
void *NatServer::nat_lan_to_wan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);
    size_t lowest_free_port = NAT_PORTS_BASE;

    for (;;) {
        auto receive = args->lan->recv();
        auto *ip_header = receive->buffer_cast<struct ip>();
        uint32_t dest_ip = ip_get_daddr(*ip_header).s_addr;

        // todo: check sub net mask

        auto send = args->wan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            uint16_t *lan_port_ptr = get_port_from_ip_datagram(*receive);

            auto value = assemble_nat_table_field(dest_ip, ntohs(*lan_port_ptr), 1);

            auto table_ptr = args->nat_reverse_table.find(value);
            uint16_t wan_port = 0;

            if (table_ptr == args->nat_reverse_table.end()) {
                wan_port = lowest_free_port;
                args->nat_table[wan_port].store(value, std::memory_order_seq_cst);
                args->nat_reverse_table.emplace(value, wan_port);
                ++lowest_free_port;
                if (lowest_free_port >= NAT_PORTS_BASE + NAT_PORTS_SIZE) {
                    cs120_abort("nat ports used up!");
                }
            } else {
                wan_port = table_ptr->second;
            }

            (*send)[Range{0, receive->size()}].copy_from_slice(*receive);

            // todo: modify src ip

            *lan_port_ptr = htonl(wan_port);

            // todo: modify checksum
        }
    }

    return nullptr;
}

void *NatServer::nat_wan_to_lan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    for (;;) {
        auto receive = args->wan->recv();

        uint16_t *wan_port_ptr = get_port_from_ip_datagram(*receive);

        auto value = args->nat_table[ntohs(*wan_port_ptr)].load(std::memory_order_seq_cst);

        if (get_nat_table_extra(value) > 0) {
            auto send = args->lan->try_send();
            if (send->empty()) {
                cs120_warn("package loss!");
            } else {
                auto lan_ip = get_nat_table_ip(value);
                auto lan_port = get_nat_table_port(value);

                (*send)[Range{0, receive->size()}].copy_from_slice(*receive);

                // modify headers
                send->buffer_cast<struct ip>()->ip_dst = in_addr{lan_ip};
                *wan_port_ptr = htonl(lan_port);

                // todo: modify checksum
            }
        }
    }

    return nullptr;
}

NatServer::NatServer(std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan) :
        lan_to_wan{}, wan_to_lan{},
        lan{std::move(lan)}, wan{std::move(wan)},
        lock{}, nat_table{NAT_PORTS_SIZE}, nat_reverse_table{} {
    pthread_create(&lan_to_wan, nullptr, nat_lan_to_wan, this);
    pthread_create(&wan_to_lan, nullptr, nat_wan_to_lan, this);
}
}
