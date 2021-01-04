#include "wire/wire.hpp"

#include <cstdio>


namespace cs120 {
uint16_t complement_checksum(Slice<uint8_t> buffer) {
    return complement_checksum_complement(complement_checksum_sum(buffer));
}

EndPoint parse_ip_address(const char *str) {
    uint8_t buffer[4]{};
    uint16_t lan_port = 0;

    if (sscanf(str, "%hhd.%hhd.%hhd.%hhd:%hd", &buffer[0], &buffer[1], &buffer[2],
               &buffer[3], &lan_port) != 5) { cs120_abort("input_format_error!"); }

    return EndPoint{*reinterpret_cast<uint32_t *>(buffer), lan_port};
}

void ETHHeader::format() const {
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
}
