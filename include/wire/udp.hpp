#ifndef CS120_UDP_HPP
#define CS120_UDP_HPP


#include <cstdint>
#include <numeric>
#include <arpa/inet.h>

#include "ipv4.hpp"


namespace cs120 {
struct UDPHeader {
private:
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;

public:
    static size_t max_payload(size_t mtu) {
        return IPV4Header::max_payload(mtu) - sizeof(UDPHeader);
    }

    UDPHeader(uint16_t src_port, uint16_t dest_port, size_t length) {
        set_src_port(src_port);
        set_dest_port(dest_port);
        set_length(length);
        set_checksum(0);
    }

    class Guard {
    private:
        const IPV4Header *ip_header;
        MutSlice<uint8_t> frame;
        MutSlice<uint8_t> inner;

    public:
        Guard() noexcept: ip_header{nullptr}, frame{}, inner{} {}

        Guard(const IPV4Header *ip_header, MutSlice<uint8_t> icmp_frame, MutSlice<uint8_t> inner) :
                ip_header{ip_header}, frame{icmp_frame} , inner{inner} {}

        Guard(Guard &&other) noexcept = default;

        Guard &operator=(Guard &&other) noexcept = default;

        MutSlice<uint8_t> &operator*() { return inner; }

        MutSlice<uint8_t> *operator->() { return &inner; }

        ~Guard() {
            auto *udp_header = reinterpret_cast<UDPHeader *>(frame.begin());
            udp_header->set_checksum_enable(complement_checksum(*ip_header, frame));
        }
    };

    static Guard generate(MutSlice<uint8_t> frame, uint8_t type_of_service,
                          uint16_t identification, uint32_t src_ip, uint32_t dest_ip,
                          uint8_t time_to_live,
                          uint16_t src_port, uint16_t dest_port, size_t len) {
        size_t udp_size = sizeof(UDPHeader) + len;

        auto udp_frame = IPV4Header::generate(frame, type_of_service, identification,
                                              IPV4Protocol::UDP, src_ip, dest_ip,
                                              0, true, false, time_to_live, udp_size);

        if (udp_frame.empty()) { return {}; }

        auto *udp_header = reinterpret_cast<UDPHeader *>(frame.begin());
        new(udp_header)UDPHeader{src_port, dest_port, udp_size};

        return Guard{
                reinterpret_cast<IPV4Header *>(frame.begin()),
                udp_frame, udp_frame[Range{sizeof(UDPHeader)}]
        };
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

    uint16_t get_src_port() const { return ntohs(src_port); }

    void set_src_port(uint16_t value) { src_port = htons(value); }

    uint16_t get_dest_port() const { return ntohs(dest_port); }

    void set_dest_port(uint16_t value) { dest_port = htons(value); }

    size_t get_header_length() const { return sizeof(UDPHeader); }

    size_t get_total_length() const { return ntohs(length); }

    size_t get_data_length() const { return get_total_length() - get_header_length(); }

    void set_length(size_t value) {
        if (value > std::numeric_limits<uint16_t>::max()) { cs120_abort("invalid total length!"); }
        length = htons(value);
    }

    uint16_t get_checksum() const { return checksum; }

    void set_checksum(uint16_t value) { checksum = value; }

    void set_checksum_enable(uint16_t value) { checksum = value == 0 ? 0xFFFF : value; }

    bool check_checksum(uint16_t value) const {
        if (checksum == 0 || value == 0) {
            return true;
        } else {
            return checksum == 0xFFFF && value == 0xFFFF;
        }
    }

    void format() const {
        printf("UDP Header {\n");
        printf("\tsource port: %hu,\n", get_src_port());
        printf("\tdestination port: %hu,\n", get_dest_port());
        printf("\tlength: %zu,\n", get_total_length());
        printf("}\n");
    }
}__attribute__((packed));


cs120_static_inline std::pair<UDPHeader *, MutSlice<uint8_t>>
udp_split(MutSlice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<UDPHeader>();
    if (header == nullptr) { return std::make_pair(nullptr, MutSlice<uint8_t>{}); }
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_pair(header, data);
}


cs120_static_inline std::pair<const UDPHeader *, Slice<uint8_t>>
udp_split(Slice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<UDPHeader>();
    if (header == nullptr) { return std::make_pair(nullptr, Slice<uint8_t>{}); }
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_pair(header, data);
}
}


#endif //CS120_UDP_HPP
