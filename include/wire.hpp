#ifndef CS120_WIRE_HPP
#define CS120_WIRE_HPP


#include <utility>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "utility.hpp"


#if defined(__linux__)
#include <netinet/if_ether.h>
#elif defined(__APPLE__)
#define ETH_ALEN 6

struct ethhdr {
    unsigned char h_dest[ETH_ALEN];
    unsigned char h_source[ETH_ALEN];
    unsigned short h_proto;
}__attribute__((packed));
#endif

struct icmp {
private:
    uint8_t type;
    uint8_t code;
    uint16_t sum;
    uint16_t ident;
    uint16_t seq;

public:
    uint8_t get_type() const { return type; }

    void set_type(uint8_t value) { type = value; }

    uint8_t get_code() const { return code; }

    void set_code(uint8_t value) { code = value; }

    uint16_t get_sum() const { return sum; }

    void set_sum(uint16_t value) { sum = value; }

    uint16_t get_ident() const { return ntohs(ident); }

    void set_ident(uint16_t value) { ident = htons(value); }

    uint16_t get_seq() const { return ntohs(seq); }

    void set_seq(uint16_t value) { seq = htons(value); }
}__attribute__((packed));

namespace cs120 {
cs120_inline uint32_t ip_get_version(const struct ip &header) {
    return header.ip_v;
}

cs120_inline uint32_t ip_get_ihl(const struct ip &header) {
    return header.ip_hl * 4u;
}

cs120_inline uint8_t ip_get_tos(const struct ip &header) {
    return header.ip_tos;
}

cs120_inline uint16_t ip_get_tot_len(const struct ip &header) {
    return ntohs(header.ip_len);
}

cs120_inline uint16_t ip_get_id(const struct ip &header) {
    return ntohs(header.ip_id);
}

cs120_inline uint16_t ip_get_offset(const struct ip &header) {
    return (ntohs(header.ip_off) & IP_OFFMASK) * 8;
}

cs120_inline bool ip_get_do_not_fragment(const struct ip &header) {
    return (ntohs(header.ip_off) & IP_DF) > 0;
}

cs120_inline bool ip_get_more_fragment(const struct ip &header) {
    return (ntohs(header.ip_off) & IP_MF) > 0;
}

cs120_inline uint8_t ip_get_ttl(const struct ip &header) {
    return header.ip_ttl;
}

cs120_inline uint8_t ip_get_protocol(const struct ip &header) {
    return header.ip_p;
}

cs120_inline uint16_t ip_get_check(const struct ip &header) {
    return header.ip_sum;
}

cs120_inline uint32_t ip_get_saddr(const struct ip &header) {
    return header.ip_src.s_addr;
}

cs120_inline uint32_t ip_get_daddr(const struct ip &header) {
    return header.ip_dst.s_addr;
}

cs120_inline uint16_t udphdr_get_source(const struct udphdr &header) {
    return ntohs(header.uh_sport);
}

cs120_inline void udphdr_set_source(struct udphdr &header, uint16_t value) {
    header.uh_sport = htons(value);
}

cs120_inline uint16_t udphdr_get_dest(const struct udphdr &header) {
    return ntohs(header.uh_dport);
}

cs120_inline void udphdr_set_dest(struct udphdr &header, uint16_t value) {
    header.uh_dport = htons(value);
}

cs120_inline uint16_t udphdr_get_len(const struct udphdr &header) {
    return ntohs(header.uh_ulen);
}

cs120_inline void udphdr_set_len(struct udphdr &header, uint16_t value) {
    header.uh_ulen = htons(value);
}

size_t get_ipv4_total_size(Slice<uint8_t> buffer);

/// RFC 1071
/// Calculates the Internet-checksum
/// Valid for the IP, ICMP, TCP or UDP header
///
/// *addr : Pointer to the Beginning of the data (checksum field must be 0)
/// len : length of the data (in bytes)
/// return : checksum in network byte order

uint16_t composite_checksum(Slice<uint8_t> buffer);

void format(const struct ethhdr &object);

void format(const struct ip &object);

void format(const struct icmp &object);

void format(const struct udphdr &object);

void format(const Slice<uint8_t> &object);

uint32_t get_local_ip();

std::pair<uint32_t, uint16_t> parse_ip_address(const char *str);

cs120_inline size_t ip_max_payload(size_t mtu) { return (mtu - sizeof(struct ip)) / 8 * 8; }

void checksum_ip(MutSlice<uint8_t> frame);

void generate_ip(MutSlice<uint8_t> frame, uint16_t identifier, uint8_t protocol,
                 uint32_t src_ip, uint32_t dest_ip, size_t len);

cs120_inline size_t icmp_max_payload(size_t mtu) {
    return ip_max_payload(mtu) - sizeof(icmp);
}

void checksum_icmp(MutSlice<uint8_t> frame, size_t icmp_size);

void generate_icmp(MutSlice<uint8_t> frame, uint16_t identifier, uint32_t src_ip, uint32_t dest_ip,
                   uint8_t type, uint16_t ident, uint16_t seq, Slice<uint8_t> data);

cs120_inline size_t udp_max_payload(size_t mtu) {
    return ip_max_payload(mtu) - sizeof(struct udphdr);
}

void checksum_udp(MutSlice<uint8_t> frame);

void generate_udp(MutSlice<uint8_t> frame, uint16_t identifier, uint32_t src_ip, uint32_t dest_ip,
                  uint16_t src_port, uint16_t dest_port, Slice<uint8_t> data);
}


#endif //CS120_WIRE_HPP
