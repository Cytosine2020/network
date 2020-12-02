#ifndef CS120_UDP_HPP
#define CS120_UDP_HPP


#include <cstdint>
#include <numeric>
#include <arpa/inet.h>

#include "ipv4.hpp"


namespace cs120 {
struct UDPHeader {
private:
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;

public:
    static size_t max_payload(size_t mtu) {
        return IPV4Header::max_payload(mtu) - sizeof(UDPHeader);
    }

    UDPHeader(uint16_t src_port, uint16_t dest_port, uint16_t length) {
        set_source_port(src_port);
        set_destination_port(dest_port);
        set_length(length);
        set_checksum(0);
    }

    static size_t generate(MutSlice<uint8_t> frame, uint16_t identifier,
                           uint32_t src_ip, uint32_t dest_ip,
                           uint16_t src_port, uint16_t dest_port, Slice<uint8_t> data) {
        size_t udp_size = sizeof(UDPHeader) + data.size();

        size_t ip_header_size = IPV4Header::generate(frame, identifier, IPV4Header::UDP,
                                                     src_ip, dest_ip, udp_size);

        auto udp_frame = frame[Range{ip_header_size}];

        auto *udp_header = reinterpret_cast<UDPHeader *>(udp_frame.begin());

        new(udp_header)UDPHeader(src_port, dest_port, udp_size);

        udp_frame[Range{sizeof(UDPHeader)}][Range{0, data.size()}].copy_from_slice(data);

        return udp_size;
    }

    static const UDPHeader *from_slice(Slice<uint8_t> data) {
        auto *result = reinterpret_cast<const UDPHeader *>(data.begin());
        if (data.size() < result->get_total_length() ||
            result->get_total_length() < sizeof(UDPHeader)) { return nullptr; }
        return result;
    }

    static UDPHeader *from_slice(MutSlice<uint8_t> data) {
        return const_cast<UDPHeader *>(from_slice(Slice<uint8_t>{data}));
    }

    uint16_t get_source_port() const { return ntohs(source_port); }

    void set_source_port(uint16_t value) { source_port = htons(value); }

    uint16_t get_destination_port() const { return ntohs(destination_port); }

    void set_destination_port(uint16_t value) { destination_port = htons(value); }

    size_t get_header_length() const { return sizeof(UDPHeader); }

    size_t get_total_length() const { return ntohs(length); }

    size_t get_data_length() const { return get_total_length() - get_header_length(); }

    void set_length(size_t value) {
        if (value > std::numeric_limits<uint16_t>::max()) { cs120_abort("invalid total length!"); }
        length = htons(value);
    }

    uint16_t get_checksum() const { return checksum; }

    void set_checksum(uint16_t value) { checksum = value; }

    void format() const {
        printf("UDP Header {\n");
        printf("\tsource port: %d,\n", get_source_port());
        printf("\tdestination port: %d,\n", get_destination_port());
        printf("\tlength: %zu,\n", get_total_length());
//    printf("\tchecksum: %d,\n", get_check());
        printf("}\n");
    }
};
}


#endif //CS120_UDP_HPP
