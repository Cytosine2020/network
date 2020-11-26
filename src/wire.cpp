#include <cstdio>

#include "wire.hpp"


void print_mac_addr(const unsigned char (&addr)[ETH_ALEN]) {
    printf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void cs120::format(const struct ethhdr &object) {
    printf("Ethernet Header {\n");
    printf("\tSource Address: ");
    print_mac_addr(object.h_source);
    printf(",\n");
    printf("\tDestination Address: ");
    print_mac_addr(object.h_dest);
    printf(",\n");
    printf("\tProtocol: %d,\n", object.h_proto);
    printf("}\n");
}

void cs120::format(const struct iphdr &object) {
    printf("IP Header {\n");
    printf("\tVersion: %d,\n", (unsigned int) object.iphdr_version);
    printf("\tInternet Header Length: %d Bytes,\n", ((unsigned int) (object.iphdr_ihl)) * 4);
    printf("\tType Of Service: %d,\n", (unsigned int) object.iphdr_tos);
    printf("\tTotal Length: %d Bytes,\n", ntohs(object.iphdr_tot_len));
    printf("\tIdentification: %d,\n", ntohs(object.iphdr_id));
    printf("\tTime To Live: %d,\n", (unsigned int) object.iphdr_ttl);
    printf("\tProtocol: %d,\n", (unsigned int) object.iphdr_protocol);
    printf("\tHeader Checksum: %d,\n", ntohs(object.iphdr_check));
    printf("\tSource IP: %s,\n", inet_ntoa(in_addr{object.iphdr_saddr}));
    printf("\tDestination IP: %s,\n", inet_ntoa(in_addr{object.iphdr_daddr}));
    printf("}\n");
}

void cs120::format(const udphdr &object) {
    printf("UDP Header {\n");
    printf("\tSource Port: %d,\n", ntohs(object.udphdr_source));
    printf("\tDestination Port: %d,\n", ntohs(object.udphdr_dest));
    printf("\tUDP Length: %d,\n", ntohs(object.udphdr_len));
    printf("\tUDP Checksum: %d,\n", ntohs(object.udphdr_check));
    printf("}\n");
}

void cs120::format(const cs120::Slice<uint8_t> &object) {
    size_t i = 0;

    for (auto &item: object) {
        if (i % 16 == 0) {
            if (i != 0) { printf("\n"); }
            printf("%.4X: ", static_cast<uint32_t>(i / 2));
        }
        printf("%.2X ", item);
        ++i;
    }

    printf("\n");
}
