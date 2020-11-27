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
            composite_checksum(&object, (object.iphdr_ihl) * 4) == 0 ? "true" : "false";

    printf("IP Header {\n");
    printf("\tversion: %d,\n", static_cast<uint32_t>(object.iphdr_version));
    printf("\tinternet header length: %d,\n", static_cast<uint32_t>(object.iphdr_ihl) * 4);
    printf("\ttype of service: %d,\n", static_cast<uint32_t>(object.iphdr_tos));
    printf("\ttotal length: %d,\n", ntohs(object.iphdr_tot_len));
    printf("\tidentification: %d,\n", ntohs(object.iphdr_id));
    printf("\ttime to live: %d,\n", static_cast<uint32_t>(object.iphdr_ttl));
    printf("\tprotocol: %d,\n", static_cast<uint32_t>(object.iphdr_protocol));
    printf("\theader checksum: %s,\n", checksum_result);
    printf("\tsource ip: %s,\n", inet_ntoa(in_addr{object.iphdr_saddr}));
    printf("\tdestination ip: %s,\n", inet_ntoa(in_addr{object.iphdr_daddr}));
    printf("}\n");
}

void format(const udphdr &object) {
    printf("UDP Header {\n");
    printf("\tsource port: %d,\n", ntohs(object.udphdr_source));
    printf("\tdestination port: %d,\n", ntohs(object.udphdr_dest));
    printf("\tlength: %d,\n", ntohs(object.udphdr_len));
    printf("\tchecksum: %d,\n", ntohs(object.udphdr_check));
    printf("}\n");
}

void format(const Slice<uint8_t> &object) {
    size_t i = 0;

    for (auto &item: object) {
        if (i % 16 == 0) { printf(&"\n%.4X: "[i == 0], static_cast<uint32_t>(i / 2)); }
        printf("%.2X ", item);
        ++i;
    }

    printf("\n");
}
}
