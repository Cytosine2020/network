#include <cstdio>

#include "wire.hpp"


void print_mac_addr(const unsigned char (&addr)[ETH_ALEN]) {
    printf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void cs120::format(const struct ethhdr &object) {
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

void cs120::format(const struct iphdr &object) {
    printf("IP Header {\n");
    printf("\tversion: %d,\n", (unsigned int) object.iphdr_version);
    printf("\tinternet header length: %d,\n", ((unsigned int) (object.iphdr_ihl)) * 4);
    printf("\ttype of service: %d,\n", (unsigned int) object.iphdr_tos);
    printf("\ttotal length: %d,\n", ntohs(object.iphdr_tot_len));
    printf("\tidentification: %d,\n", ntohs(object.iphdr_id));
    printf("\ttime to live: %d,\n", (unsigned int) object.iphdr_ttl);
    printf("\tprotocol: %d,\n", (unsigned int) object.iphdr_protocol);
    printf("\theader checksum: %d,\n", ntohs(object.iphdr_check));
    printf("\tsource ip: %s,\n", inet_ntoa(in_addr{object.iphdr_saddr}));
    printf("\tdestination ip: %s,\n", inet_ntoa(in_addr{object.iphdr_daddr}));
    printf("}\n");
}

void cs120::format(const udphdr &object) {
    printf("UDP Header {\n");
    printf("\tsource port: %d,\n", ntohs(object.udphdr_source));
    printf("\tdestination port: %d,\n", ntohs(object.udphdr_dest));
    printf("\tlength: %d,\n", ntohs(object.udphdr_len));
    printf("\tchecksum: %d,\n", ntohs(object.udphdr_check));
    printf("}\n");
}

void cs120::format(const cs120::Slice<uint8_t> &object) {
    size_t i = 0;

    for (auto &item: object) {
        if (i % 16 == 0) { printf(&"\n%.4X: "[i == 0], static_cast<uint32_t>(i / 2)); }
        printf("%.2X ", item);
        ++i;
    }

    printf("\n");
}
