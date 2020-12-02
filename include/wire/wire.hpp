#ifndef CS120_WIRE_HPP
#define CS120_WIRE_HPP


#include <cstdint>
#include <utility>
#include <arpa/inet.h>

#include "utility.hpp"


namespace cs120 {
/// RFC 1071
/// Calculates the Internet-checksum
/// Valid for the IP, ICMP, TCP or UDP header
///
/// *addr : Pointer to the Beginning of the data (checksum field must be 0)
/// len : length of the data (in bytes)
/// return : checksum in network byte order
uint16_t complement_checksum(Slice<uint8_t> buffer);

uint32_t get_local_ip();

std::pair<uint32_t, uint16_t> parse_ip_address(const char *str);

using MacAddr = uint8_t[6];

cs120_static_inline void print_mac_addr(const MacAddr &addr) {
    printf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

struct ETHHeader {
    MacAddr destination_mac;
    MacAddr source_mac;
    uint16_t protocol;

    static const ETHHeader *from_slice(Slice<uint8_t> data) {
        return reinterpret_cast<const ETHHeader *>(data.begin());
    }

    static ETHHeader *from_slice(MutSlice<uint8_t> data) {
        return const_cast<ETHHeader *>(from_slice(Slice<uint8_t>{data}));
    }

    void format() const {
        printf("Ethernet Header {\n");
        printf("\tdestination address: ");
        print_mac_addr(destination_mac);
        printf(",\n");
        printf("\tsource address: ");
        print_mac_addr(source_mac);
        printf(",\n");
        printf("\tprotocol: %d,\n", protocol);
        printf("}\n");
    }
}__attribute__((packed));
}


#endif //CS120_WIRE_HPP
