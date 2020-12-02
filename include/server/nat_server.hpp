#ifndef CS120_NAT_SERVER_HPP
#define CS120_NAT_SERVER_HPP


#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

#include "device/base_socket.hpp"
#include "wire/wire.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"
#include "wire/udp.hpp"


namespace cs120 {
class NatServer {
private:
    static void *nat_lan_to_wan(void *args_);

    static void *nat_wan_to_lan(void *args_);

    static uint16_t get_src_port_from_ip_datagram(MutSlice<uint8_t> datagram) {
        auto *ip_header = datagram.buffer_cast<IPV4Header>();
        if (ip_header == nullptr) { return 0; }
        auto ip_data = datagram[Range{ip_header->get_header_length(),
                                      ip_header->get_total_length()}];

        switch (ip_header->get_protocol()) {
            case IPV4Header::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr) { return 0; }
                return icmp_header->get_identification();
            }
            case IPV4Header::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                if (udp_header == nullptr) { return 0; }
                return udp_header->get_source_port();
            }
            default:
                return 0;
        }
    }

    static void set_src_port_from_ip_datagram(MutSlice<uint8_t> datagram, uint16_t port) {
        auto *ip_header = datagram.buffer_cast<IPV4Header>();
        if (ip_header == nullptr) { return; }
        auto ip_data = datagram[Range{ip_header->get_header_length(),
                                      ip_header->get_total_length()}];

        switch (ip_header->get_protocol()) {
            case IPV4Header::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr) { return; }
                icmp_header->set_identification(port);
                icmp_header->set_checksum(0);
                icmp_header->set_checksum(complement_checksum(ip_data));
                return;
            }
            case IPV4Header::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                if (udp_header == nullptr) { return; }
                udp_header->set_source_port(port);
                udp_header->set_checksum(0);
                return;
            }
            default:
                return;
        }
    }

    static uint16_t get_dest_port_from_ip_datagram(MutSlice<uint8_t> datagram) {
        auto *ip_header = datagram.buffer_cast<IPV4Header>();
        if (ip_header == nullptr) { return 0; }
        auto ip_data = datagram[Range{ip_header->get_header_length(),
                                      ip_header->get_total_length()}];

        switch (ip_header->get_protocol()) {
            case IPV4Header::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr) { return 0; }
                return icmp_header->get_identification();
            }
            case IPV4Header::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                if (udp_header == nullptr) { return 0; }
                return udp_header->get_destination_port();
            }
            default:
                return 0;
        }
    }

    static void set_dest_port_from_ip_datagram(MutSlice<uint8_t> datagram, uint16_t port) {
        auto *ip_header = datagram.buffer_cast<IPV4Header>();
        if (ip_header == nullptr) { return; }
        auto ip_data = datagram[Range{ip_header->get_header_length(),
                                      ip_header->get_total_length()}];

        switch (ip_header->get_protocol()) {
            case IPV4Header::ICMP: {
                auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
                if (icmp_header == nullptr) { return; }
                icmp_header->set_identification(port);
                icmp_header->set_checksum(0);
                icmp_header->set_checksum(complement_checksum(ip_data));
                return;
            }
            case IPV4Header::UDP: {
                auto *udp_header = ip_data.buffer_cast<UDPHeader>();
                if (udp_header == nullptr) { return; }
                udp_header->set_destination_port(port);
                udp_header->set_checksum(0);
                return;
            }
            default:
                return;
        }
    }

    pthread_t lan_to_wan, wan_to_lan;
    std::unique_ptr<BaseSocket> lan, wan;
    Array<std::atomic<uint64_t>> nat_table;
    std::unordered_map<uint64_t, uint16_t> nat_reverse_table;
    size_t lowest_free_port;
    uint32_t ip_addr;

public:
    static const uint16_t NAT_PORTS_BASE = 45678;
    static const uint16_t NAT_PORTS_SIZE = 1024;

    static uint32_t get_nat_table_ip(uint64_t value) {
        return (value & 0x00000000FFFFFFFFLLU) >> 0u;
    }

    static uint16_t get_nat_table_port(uint64_t value) {
        return (value & 0x0000FFFF00000000LLU) >> 32u;
    }

    static uint16_t get_nat_table_extra(uint64_t value) {
        return (value & 0xFFFF000000000000LLU) >> 48u;
    }

    static uint64_t assemble_nat_table_field(uint32_t ip, uint16_t port) {
        return static_cast<uint64_t>(ip) |
               (static_cast<uint64_t>(port) << 32u) |
               (static_cast<uint64_t>(1) << 48u);
    }

    NatServer(uint32_t ip_addr, std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan,
              const Array<std::pair<uint32_t, uint16_t>> &map_addr);

    NatServer(NatServer &&other) = delete;

    NatServer &operator=(NatServer &&other) = delete;

    ~NatServer() {
        pthread_join(lan_to_wan, nullptr);
        pthread_join(wan_to_lan, nullptr);
    };
};
}


#endif //CS120_NAT_SERVER_HPP
