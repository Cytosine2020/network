#ifndef CS120_ICMP_HPP
#define CS120_ICMP_HPP


#include <cstdint>
#include <arpa/inet.h>

#include "utility.hpp"
#include "ipv4.hpp"


namespace cs120 {
enum class ICMPType : uint8_t {
    EchoReply = 0,
    EchoRequest = 8,
};

struct ICMPHeader {
private:
    ICMPType type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identification;
    uint16_t sequence;

public:
    static size_t max_payload(size_t mtu) {
        return IPV4Header::max_payload(mtu) - sizeof(ICMPHeader);
    }

    ICMPHeader(ICMPType type, uint16_t identification, uint16_t sequence) {
        set_type(type);
        set_code(0);
        set_checksum(0);
        set_identification(identification);
        set_sequence(sequence);
    }

    static size_t generate(MutSlice<uint8_t> frame, uint16_t identifier,
                           uint32_t src_ip, uint32_t dest_ip, ICMPType type,
                           uint16_t identification, uint16_t sequence, Slice<uint8_t> data) {
        size_t icmp_size = sizeof(ICMPHeader) + data.size();

        size_t ip_header_size = IPV4Header::generate(frame, identifier, IPV4Protocol::ICMP,
                                                     src_ip, dest_ip, icmp_size);

        if (ip_header_size == 0) { return 0; }

        size_t ip_datagram_size = icmp_size + ip_header_size;

        if (frame.size() < ip_datagram_size) { return 0; }

        auto icmp_frame = frame[Range{ip_header_size}][Range{0, icmp_size}];

        auto *icmp_header = reinterpret_cast<ICMPHeader *>(icmp_frame.begin());

        new(icmp_header)ICMPHeader{type, identification, sequence};

        icmp_frame[Range{sizeof(ICMPHeader)}].copy_from_slice(data);

        icmp_header->set_checksum(complement_checksum(icmp_frame));

        return ip_datagram_size;
    }

    static const ICMPHeader *from_slice(Slice<uint8_t> data) {
        return reinterpret_cast<const ICMPHeader *>(data.begin());
    }

    static ICMPHeader *from_slice(MutSlice<uint8_t> data) {
        return const_cast<ICMPHeader *>(from_slice(Slice<uint8_t>{data}));
    }

    ICMPType get_type() const { return type; }

    void set_type(ICMPType value) { type = value; }

    uint8_t get_code() const { return code; }

    void set_code(uint8_t value) { code = value; }

    uint16_t get_checksum() const { return checksum; }

    void set_checksum(uint16_t value) { checksum = value; }

    uint16_t get_identification() const { return ntohs(identification); }

    void set_identification(uint16_t value) { identification = htons(value); }

    uint16_t get_sequence() const { return ntohs(sequence); }

    void set_sequence(uint16_t value) { sequence = htons(value); }

    void format() const {
        printf("ICMP Header {\n");
        printf("\ttype: %hhu,\n", static_cast<uint8_t>(get_type()));
        printf("\tcode: %hhu,\n", get_code());
        printf("\tidentifier: %hu,\n", get_identification());
        printf("\tsequence: %hu,\n", get_sequence());
        printf("}\n");
    }
}__attribute__((packed));
}


#endif //CS120_ICMP_HPP
