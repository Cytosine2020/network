#include <cstdio>

#include "wire.hpp"


namespace cs120 {
uint16_t composite_checksum(const void *addr_, size_t len) {
    const uint16_t *addr = reinterpret_cast<const uint16_t *>(addr_);
    if (len % 2 != 0) { cs120_abort("length is not multiple of 2!"); }

    uint32_t sum = 0;

    for (size_t i = 0; i * 2 < len; ++i) { sum += addr[i]; }

    return static_cast<uint16_t>(~((sum & 0xffffu) + (sum >> 16u)));
}

void print_mac_addr(const unsigned char (&addr)[ETH_ALEN]) {
    printf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void format(const struct ethhdr &object) {
    printf("Ethernet Header {\n");
    printf("\tsource Address: ");
    print_mac_addr(object.h_source);
    printf(",\n");
    printf("\tdestination Address: ");
    print_mac_addr(object.h_dest);
    printf(",\n");
    printf("\tprotocol: %d,\n", object.h_proto);
    printf("}\n");
}

void format(const struct iphdr &object) {
    const char *checksum_result =
            composite_checksum(&object, iphdr_get_ihl(object) * 4) == 0 ? "true" : "false";

    printf("IP Header {\n");
    printf("\tversion: %d,\n", static_cast<uint32_t>(iphdr_get_version(object)));
    printf("\tinternet header length: %d,\n", static_cast<uint32_t>(iphdr_get_ihl(object)) * 4);
    printf("\ttype of service: %d,\n", static_cast<uint32_t>(iphdr_get_tos(object)));
    printf("\ttotal length: %d,\n", iphdr_get_tot_len(object));
    printf("\tidentification: %d,\n", iphdr_get_id(object));
    printf("\ttime to live: %d,\n", static_cast<uint32_t>(iphdr_get_ttl(object)));
    printf("\tprotocol: %d,\n", static_cast<uint32_t>(iphdr_get_protocol(object)));
    printf("\theader checksum: %s,\n", checksum_result);
    printf("\tsource ip: %s,\n", inet_ntoa(iphdr_get_saddr(object)));
    printf("\tdestination ip: %s,\n", inet_ntoa(iphdr_get_daddr(object)));
    printf("}\n");
}

void format(const udphdr &object) {
    printf("UDP Header {\n");
    printf("\tsource port: %d,\n", udphdr_get_source(object));
    printf("\tdestination port: %d,\n", udphdr_get_dest(object));
    printf("\tlength: %d,\n", udphdr_get_len(object));
    printf("\tchecksum: %d,\n", udphdr_get_check(object));
    printf("}\n");
}

void format(const Slice<uint8_t> &object) {
    size_t i = 0;

    for (auto &item: object) {
        if (i % 32 == 0) { printf(&"\n%.4X: "[i == 0], static_cast<uint32_t>(i)); }
        printf("%.2X ", item);
        ++i;
    }

    printf("\n");
}
}
