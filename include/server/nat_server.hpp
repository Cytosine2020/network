#ifndef CS120_NAT_SERVER_HPP
#define CS120_NAT_SERVER_HPP


#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

#include "device/mod.hpp"
#include "wire.hpp"


namespace cs120 {
class NatServer {
private:
    static const uint16_t NAT_PORTS_BASE = 45678;
    static const uint16_t NAT_PORTS_SIZE = 1024;

    static void *nat_lan_to_wan(void *args_);

    static void *nat_wan_to_lan(void *args_);

    static uint32_t get_nat_table_ip(uint64_t value) {
        return (value & 0x00000000FFFFFFFFLLU) >> 0u;
    }

    static uint16_t get_nat_table_port(uint64_t value) {
        return (value & 0x0000FFFF00000000LLU) >> 32u;
    }

    static uint16_t get_nat_table_extra(uint64_t value) {
        return (value & 0xFFFF000000000000LLU) >> 48u;
    }

    static uint64_t assemble_nat_table_field(uint32_t ip, uint16_t port, uint16_t extra) {
        return static_cast<uint64_t>(ip) |
               (static_cast<uint64_t>(port) << 32u) |
               (static_cast<uint64_t>(extra) << 48u);
    }

    static uint16_t *get_port_from_ip_datagram(MutSlice<uint8_t> datagram) {
        auto *ip_header = datagram.buffer_cast<struct ip>();
        auto ip_data = datagram[Range{ip_get_ihl(*ip_header), ip_get_tot_len(*ip_header)}];

        switch (ip_get_protocol(*ip_header)) {
            case 1:
                return nullptr;
            case 17:
            {
                auto *udp_header = ip_data.buffer_cast<struct udphdr>();
                if (udp_header == nullptr) { return nullptr; }
                return &udp_header->uh_dport;
            }
            default:
                return nullptr;
        }
    }

    pthread_t lan_to_wan, wan_to_lan;
    std::unique_ptr<BaseSocket> lan, wan;
    std::mutex lock;
    Array<std::atomic<uint64_t>> nat_table;
    std::unordered_map<uint64_t, uint16_t> nat_reverse_table;

public:
    NatServer(std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan);

    NatServer(NatServer &&other) = delete;

    NatServer &operator=(NatServer &&other) = delete;

    ~NatServer() = default;
};
}


#endif //CS120_NAT_SERVER_HPP
