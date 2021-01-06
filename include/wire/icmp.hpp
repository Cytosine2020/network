#ifndef CS120_ICMP_HPP
#define CS120_ICMP_HPP


#include <cstdint>
#include <arpa/inet.h>

#include "utility.hpp"
#include "ipv4.hpp"


namespace cs120 {
enum class ICMPType : uint8_t {
    EchoReply = 0,
    Unreachable = 3,
    EchoRequest = 8,
};

struct ICMPHeader {
private:
    ICMPType type;
    uint8_t code;
    uint16_t checksum;

public:
    static size_t max_payload(size_t mtu) {
        return IPV4Header::max_payload(mtu) - sizeof(ICMPHeader);
    }

    ICMPHeader(ICMPType type, uint8_t code) {
        set_type(type);
        set_code(code);
        set_checksum(0);
    }

    class Guard {
    private:
        MutSlice<uint8_t> frame;
        MutSlice<uint8_t> inner;

    public:
        Guard() noexcept: frame{}, inner{} {}

        Guard(MutSlice<uint8_t> icmp_frame, MutSlice<uint8_t> inner) :
                frame{icmp_frame} , inner{inner} {}

        Guard(Guard &&other) noexcept = default;

        Guard &operator=(Guard &&other) noexcept = default;

        MutSlice<uint8_t> &operator*() { return inner; }

        MutSlice<uint8_t> *operator->() { return &inner; }

        ~Guard() {
            auto *icmp_header = reinterpret_cast<ICMPHeader *>(frame.begin());
            icmp_header->set_checksum(complement_checksum(frame));
        }
    };

    static Guard generate(MutSlice<uint8_t> frame, uint8_t type_of_service,
                          uint16_t identification, uint32_t src_ip, uint32_t dest_ip,
                          uint8_t time_to_live, ICMPType type, uint8_t code, size_t len) {
        auto icmp_frame = IPV4Header::generate(frame, type_of_service, identification,
                                               IPV4Protocol::ICMP, src_ip, dest_ip,
                                               0, true, false, time_to_live,
                                               sizeof(ICMPHeader) + len);
        if (icmp_frame.empty()) { return {}; }

        auto *icmp_header = reinterpret_cast<ICMPHeader *>(icmp_frame.begin());
        new(icmp_header)ICMPHeader{type, code};

        return Guard{icmp_frame, icmp_frame[Range{sizeof(ICMPHeader)}]};
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

    size_t get_header_length() const { return sizeof(ICMPHeader); }

    void format() const {
        printf("ICMP Header {\n");
        printf("\ttype: %hhu,\n", static_cast<uint8_t>(get_type()));
        printf("\tcode: %hhu,\n", get_code());
        printf("}\n");
    }
}__attribute__((packed));


cs120_static_inline std::pair<ICMPHeader *, MutSlice<uint8_t>>
icmp_split(MutSlice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<ICMPHeader>();
    if (header == nullptr) { return std::make_pair(nullptr, MutSlice<uint8_t>{}); }
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_pair(header, data);
}


cs120_static_inline std::pair<const ICMPHeader *, Slice<uint8_t>>
icmp_split(Slice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<ICMPHeader>();
    if (header == nullptr) { return std::make_pair(nullptr, Slice<uint8_t>{}); }
    auto data = datagram[Range{header->get_header_length()}];
    return std::make_pair(header, data);
}


struct ICMPEcho : public IntoSliceTrait<ICMPEcho> {
private:
    uint16_t identification;
    uint16_t sequence;
    uint16_t src_port;
    uint16_t dest_port;

public:
    static const ICMPEcho *from_slice(Slice<uint8_t> data) {
        return reinterpret_cast<const ICMPEcho *>(data.begin());
    }

    static ICMPEcho *from_slice(MutSlice<uint8_t> data) {
        return const_cast<ICMPEcho *>(from_slice(Slice<uint8_t>{data}));
    }

    ICMPEcho(uint16_t identification, uint16_t sequence,
             uint16_t src_port, uint16_t dest_port) {
        set_identification(identification);
        set_sequence(sequence);
        set_src_port(src_port);
        set_dest_port(dest_port);
    }

    uint16_t get_identification() const { return ntohs(identification); }

    void set_identification(uint16_t value) { identification = htons(value); }

    uint16_t get_sequence() const { return ntohs(sequence); }

    void set_sequence(uint16_t value) { sequence = htons(value); }

    uint16_t get_src_port() const { return ntohs(src_port); }

    void set_src_port(uint16_t value) { src_port = htons(value); }

    uint16_t get_dest_port() const { return ntohs(dest_port); }

    void set_dest_port(uint16_t value) { dest_port = htons(value); }

    void format() const {
        printf("ICMP Ping Option {\n");
        printf("\tidentifier: %hu,\n", get_identification());
        printf("\tsequence: %hu,\n", get_sequence());
        printf("\tsource port: %hu,\n", get_src_port());
        printf("\tdestination port: %hu,\n", get_dest_port());
        printf("}\n");
    }
}__attribute__((packed));


struct ICMPUnreachable : public IntoSliceTrait<ICMPUnreachable> {
public:
    enum Code : uint8_t {
        DatagramTooBig = 4,
    };

private:
    uint16_t unused = 0;
    uint16_t next_hop_mtu;

public:
    explicit ICMPUnreachable(uint16_t next_hop_mtu) : next_hop_mtu{0} {
        set_next_hop_mtu(next_hop_mtu);
    }

    uint16_t get_next_hop_mtu() const { return ntohs(next_hop_mtu); }

    void set_next_hop_mtu(uint16_t value) { next_hop_mtu = htons(value); }

    uint16_t get_unused() const { return unused; }
}__attribute__((packed));
}


#endif //CS120_ICMP_HPP
