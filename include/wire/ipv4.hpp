#ifndef CS120_IPV4_HPP
#define CS120_IPV4_HPP


#include <cstdint>
#include <numeric>
#include <arpa/inet.h>

#include "utility.hpp"
#include "wire.hpp"


namespace cs120 {
enum class IPV4Protocol : uint8_t {
    ICMP = 1,
    TCP = 6,
    UDP = 17,
};


struct IPV4Header : public IntoSliceTrait<IPV4Header> {
private:
    static constexpr uint8_t VERSION = 4;

    static constexpr uint16_t FRAGMENT_RESERVE = 0x8000;
    static constexpr uint16_t DO_NOT_FRAGMENT = 0x4000;
    static constexpr uint16_t MORE_FRAGMENT = 0x2000;
    static constexpr uint16_t FRAGMENT_OFFSET = 0x1fff;

#if BYTE_ORDER == LITTLE_ENDIAN
    uint8_t header_length: 4, version: 4;
#elif BYTE_ORDER == BIG_ENDIAN
    uint8_t version: 4, header_length: 4;
#else
#error "fix byte order"
#endif
    uint8_t type_of_service;
    uint16_t total_length;
    uint16_t identification;
    uint16_t fragment_offset;
    uint8_t time_to_live;
    IPV4Protocol protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;

public:
    static size_t max_payload(size_t mtu) { return (mtu - sizeof(IPV4Header)) / 8 * 8; }

    IPV4Header(uint8_t type_of_service, uint16_t identification, IPV4Protocol protocol,
               uint32_t src_ip, uint32_t dest_ip,
               size_t offset, bool do_not_fragment, bool more_fragment,
               uint8_t time_to_live, size_t len) {
        set_header_length(sizeof(IPV4Header));
        set_version();
        set_type_of_service(type_of_service);
        set_total_length(sizeof(IPV4Header) + len);
        set_identification(identification);
        set_fragment(offset, do_not_fragment, more_fragment);
        set_time_to_live(time_to_live);
        set_protocol(protocol);
        set_checksum(0);
        set_src_ip(src_ip);
        set_dest_ip(dest_ip);
    }

    static MutSlice<uint8_t> generate(MutSlice<uint8_t> frame, uint8_t type_of_service,
                                      uint16_t identification, IPV4Protocol protocol,
                                      uint32_t src_ip, uint32_t dest_ip,
                                      size_t offset, bool do_not_fragment, bool more_fragment,
                                      uint8_t time_to_live, size_t len) {
        if (frame.size() < sizeof(IPV4Header) + len) { return {}; }

        auto *ip_header = reinterpret_cast<IPV4Header *>(frame.begin());
        new(ip_header)IPV4Header{type_of_service, identification, protocol, src_ip, dest_ip,
                                 offset, do_not_fragment, more_fragment, time_to_live, len};

        ip_header->set_checksum(complement_checksum(ip_header->into_slice()));

        return frame[Range{sizeof(IPV4Header)}][Range{0, len}];
    }

    static const IPV4Header *from_slice(Slice<uint8_t> data) {
        auto *result = reinterpret_cast<const IPV4Header *>(data.begin());
        if (data.size() < result->get_total_length() ||
            result->get_total_length() < result->get_header_length() ||
            result->get_header_length() < sizeof(IPV4Header) ||
            result->get_version() != VERSION) { return nullptr; }

        return result;
    }

    static IPV4Header *from_slice(MutSlice<uint8_t> data) {
        return const_cast<IPV4Header *>(from_slice(Slice<uint8_t>{data}));
    }

    uint8_t get_version() const { return version; }

    void set_version() { version = VERSION; }

    size_t get_header_length() const { return header_length * 4u; }

    void set_header_length(size_t value) {
        if (value % 4 != 0 || value > 0b111100) { cs120_abort("invalid header length!"); }
        header_length = value / 4;
    }

    uint8_t get_type_of_service() const { return type_of_service; }

    void set_type_of_service(uint8_t value) { type_of_service = value; }

    size_t get_total_length() const { return ntohs(total_length); }

    size_t get_data_length() const { return get_total_length() - get_header_length(); }

    void set_total_length(size_t value) {
        if (value > std::numeric_limits<uint16_t>::max()) { cs120_abort("invalid total length!"); }
        total_length = htons(value);
    }

    uint16_t get_identification() const { return ntohs(identification); }

    void set_identification(uint16_t value) { identification = htons(value); }

    uint16_t get_fragment_offset() const { return (ntohs(fragment_offset) & FRAGMENT_OFFSET) * 8; }

    bool get_do_not_fragment() const { return (ntohs(fragment_offset) & DO_NOT_FRAGMENT) > 0; }

    bool get_more_fragment() const { return (ntohs(fragment_offset) & MORE_FRAGMENT) > 0; }

    uint16_t get_fragment() const { return ntohs(fragment_offset); }

    void set_fragment(size_t offset, bool do_not_fragment, bool more_fragment) {
        if (offset % 8 != 0 || offset > std::numeric_limits<uint16_t>::max()) {
            cs120_abort("invalid fragment offset!");
        }

        uint16_t value = 0;
        value |= (offset / 8) & FRAGMENT_OFFSET;
        if (do_not_fragment) { value |= DO_NOT_FRAGMENT; }
        if (more_fragment) { value |= MORE_FRAGMENT; }

        fragment_offset = htons(value);
    }

    uint8_t get_time_to_live() const { return time_to_live; }

    void set_time_to_live(uint8_t value) { time_to_live = value; }

    IPV4Protocol get_protocol() const { return protocol; }

    void set_protocol(IPV4Protocol value) { protocol = value; }

    uint16_t get_checksum() const { return checksum; }

    void set_checksum(uint16_t value) { checksum = value; }

    uint32_t get_src_ip() const { return src_ip; }

    void set_src_ip(uint32_t value) { src_ip = value; }

    uint32_t get_dest_ip() const { return dest_ip; }

    void set_dest_ip(uint32_t value) { dest_ip = value; }

    void format() const {
        printf("IP Header {\n");
        printf("\tversion: %hhu,\n", get_version());
        printf("\theader length: %zu,\n", get_header_length());
        printf("\ttype of service: %hhu,\n", get_type_of_service());
        printf("\ttotal length: %zu,\n", get_total_length());
        printf("\tidentification: %hu,\n", get_identification());
        printf("\tdo not fragment: %s,\n", bool_to_string(get_do_not_fragment()));
        printf("\tmore fragment: %s,\n", bool_to_string(get_more_fragment()));
        printf("\tfragment offset: %hu,\n", get_fragment_offset());
        printf("\ttime to live: %hhu,\n", get_time_to_live());
        printf("\tprotocol: %hhu,\n", static_cast<uint8_t>(get_protocol()));
        printf("\theader checksum: %s,\n", bool_to_string(complement_checksum(into_slice()) == 0));
        printf("\tsource ip: %s,\n", inet_ntoa(in_addr{get_src_ip()}));
        printf("\tdestination ip: %s,\n", inet_ntoa(in_addr{get_dest_ip()}));
        printf("}\n");
    }
}__attribute__((packed));


struct IPV4PseudoHeader : public IntoSliceTrait<IPV4PseudoHeader> {
private:
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t padding = 0;
    IPV4Protocol protocol;
    uint16_t data_length;

public:
    IPV4PseudoHeader(uint32_t source_ip, uint32_t destination_ip,
                     IPV4Protocol protocol, uint16_t data_length) {
        set_src_ip(source_ip);
        set_dest_ip(destination_ip);
        set_protocol(protocol);
        set_data_length(data_length);
    }

    explicit IPV4PseudoHeader(const IPV4Header &other) {
        set_src_ip(other.get_src_ip());
        set_dest_ip(other.get_dest_ip());
        set_protocol(other.get_protocol());
        set_data_length(other.get_data_length());
    }

    uint32_t get_src_ip() const { return src_ip; }

    void set_src_ip(uint32_t value) { src_ip = value; }

    uint32_t get_dest_ip() const { return dest_ip; }

    void set_dest_ip(uint32_t value) { dest_ip = value; }

    IPV4Protocol get_protocol() const { return protocol; }

    void set_protocol(IPV4Protocol value) { protocol = value; }

    size_t get_data_length() const { return ntohs(data_length); }

    void set_data_length(size_t value) {
        if (value > std::numeric_limits<uint16_t>::max()) { cs120_abort("invalid total length!"); }
        data_length = htons(value);
    }

    uint8_t get_padding() const { return padding; }
}__attribute__((packed));


cs120_static_inline uint16_t complement_checksum(const IPV4Header &ip_header, Slice<uint8_t> b) {
    IPV4PseudoHeader pseudo{ip_header};
    uint32_t sum = complement_checksum_sum(pseudo.into_slice()) + complement_checksum_sum(b);
    return complement_checksum_complement(sum);
}


cs120_static_inline std::tuple<IPV4Header *, MutSlice<uint8_t>, MutSlice<uint8_t>>
ipv4_split(MutSlice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<IPV4Header>();
    if (header == nullptr) {
        return std::make_tuple(nullptr, MutSlice<uint8_t>{}, MutSlice<uint8_t>{});
    }
    auto option = datagram[Range{sizeof(IPV4Header), header->get_header_length()}];
    auto data = datagram[Range{header->get_header_length(), header->get_total_length()}];
    return std::make_tuple(header, option, data);
}


cs120_static_inline std::tuple<const IPV4Header *, Slice<uint8_t>, Slice<uint8_t>>
ipv4_split(Slice<uint8_t> datagram) {
    auto *header = datagram.buffer_cast<IPV4Header>();
    if (header == nullptr) {
        return std::make_tuple(nullptr, Slice<uint8_t>{}, Slice<uint8_t>{});
    }
    auto option = datagram[Range{sizeof(IPV4Header), header->get_header_length()}];
    auto data = datagram[Range{header->get_header_length(), header->get_total_length()}];
    return std::make_tuple(header, option, data);
}
}


#endif //CS120_IPV4_HPP
