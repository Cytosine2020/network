#ifndef CS120_WIRE_HPP
#define CS120_WIRE_HPP


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
    uint8_t type;
    uint8_t code;
    uint16_t sum;
    uint16_t ident;
    uint16_t seq;
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
    return ntohs(header.ip_off & IP_OFFMASK);
}

cs120_inline bool ip_get_do_not_fragment(const struct ip &header) {
    return ntohs(header.ip_off & IP_DF) > 0;
}

cs120_inline bool ip_get_more_fragment(const struct ip &header) {
    return ntohs(header.ip_off & IP_MF) > 0;
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

cs120_inline struct in_addr ip_get_saddr(const struct ip &header) {
    return in_addr{header.ip_src};
}

cs120_inline struct in_addr ip_get_daddr(const struct ip &header) {
    return in_addr{header.ip_dst};
}

cs120_inline uint16_t udphdr_get_source(const struct udphdr &header) {
    return ntohs(header.uh_sport);
}

cs120_inline uint16_t udphdr_get_dest(const struct udphdr &header) {
    return ntohs(header.uh_dport);
}

cs120_inline uint16_t udphdr_get_len(const struct udphdr &header) {
    return ntohs(header.uh_ulen);
}

cs120_inline uint16_t udphdr_get_check(const struct udphdr &header) {
    return ntohs(header.uh_sum);
}

size_t get_ipv4_data_size(Slice<uint8_t> buffer);

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

void format(const struct udphdr &object);

void format(const Slice<uint8_t> &object);

uint32_t get_local_ip();

void generate_ip(MutSlice<uint8_t> frame, uint32_t src, uint32_t dest, size_t len);

void generate_icmp_request(MutSlice<uint8_t> frame, uint32_t src, uint32_t dest,
                           uint16_t seq, Slice<uint8_t> data);

void generate_icmp_reply(MutSlice<uint8_t> frame, uint16_t seq, Slice<uint8_t> data);
}


#endif //CS120_WIRE_HPP
