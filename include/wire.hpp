#ifndef CS120_WIRE_HPP
#define CS120_WIRE_HPP


#include "utility.hpp"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>


#if defined(__linux__)
#include <netinet/if_ether.h>

#define iphdr_version version
#define iphdr_ihl ihl
#define iphdr_tos tos
#define iphdr_tot_len tot_len
#define iphdr_id id
#define iphdr_ttl ttl
#define iphdr_protocol protocol
#define iphdr_check check
#define iphdr_saddr saddr
#define iphdr_daddr daddr
#define udphdr_source source
#define udphdr_dest dest
#define udphdr_len len
#define udphdr_check check
#elif defined(__APPLE__)
#define ETH_ALEN 6

struct ethhdr {
    unsigned char h_dest[ETH_ALEN];
    unsigned char h_source[ETH_ALEN];
    unsigned short h_proto;
}__attribute__((packed));

#define iphdr ip
#define iphdr_version ip_v
#define iphdr_ihl ip_hl
#define iphdr_tos ip_tos
#define iphdr_tot_len ip_len
#define iphdr_id ip_id
#define iphdr_ttl ip_ttl
#define iphdr_protocol ip_p
#define iphdr_check ip_sum
#define iphdr_saddr ip_src
#define iphdr_daddr ip_dst
#define udphdr_source uh_sport
#define udphdr_dest uh_dport
#define udphdr_len uh_ulen
#define udphdr_check uh_sum
#endif

namespace cs120 {

/// RFC 1071
/// Calculates the Internet-checksum
/// Valid for the IP, ICMP, TCP or UDP header
///
/// *addr : Pointer to the Beginning of the data (checksum field must be 0)
/// len : length of the data (in bytes)
/// return : checksum in network byte order

uint16_t composite_checksum(const uint16_t *addr, size_t len);

void format(const struct ethhdr &object);

void format(const struct iphdr &object);

void format(const struct udphdr &object);

void format(const Slice<uint8_t> &object);
}


#endif //CS120_WIRE_HPP
