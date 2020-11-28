#ifndef CS120_WIRE_HPP
#define CS120_WIRE_HPP


#include "utility.hpp"

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>


#if defined(__linux__)
#include <netinet/if_ether.h>

#define iphdr_version(header) ((header).version)
#define iphdr_ihl(header) ((header).ihl)
#define iphdr_tos(header) ((header).tos)
#define iphdr_tot_len(header) (ntohs((header).tot_len))
#define iphdr_id(header) (ntohs((header).id))
#define iphdr_ttl(header) ((header).ttl)
#define iphdr_protocol(header) ((header).protocol)
#define iphdr_check(header) ((header).check)
#define iphdr_saddr(header) ((header).saddr)
#define iphdr_daddr(header) ((header).daddr)
#define udphdr_source(header) (ntohs((header).source))
#define udphdr_dest(header) (ntohs((header).dest))
#define udphdr_len(header) (ntohs((header).len))
#define udphdr_check(header) (ntohs((header).check))
#elif defined(__APPLE__)
#define ETH_ALEN 6

struct ethhdr {
    unsigned char h_dest[ETH_ALEN];
    unsigned char h_source[ETH_ALEN];
    unsigned short h_proto;
}__attribute__((packed));

#define iphdr ip
#define iphdr_version(header) ((header).ip_v)
#define iphdr_ihl(header) ((header).ip_hl)
#define iphdr_tos(header) ((header).ip_tos)
#define iphdr_tot_len(header) (ntohs((header).ip_len))
#define iphdr_id(header) (ntohs((header).ip_id))
#define iphdr_ttl(header) ((header).ip_ttl)
#define iphdr_protocol(header) ((header).ip_p)
#define iphdr_check(header) ((header).ip_sum)
#define iphdr_saddr(header) ((header).ip_src)
#define iphdr_daddr(header) ((header).ip_dst)
#define udphdr_source(header) (ntohs((header).uh_sport))
#define udphdr_dest(header) (ntohs((header).uh_dport))
#define udphdr_len(header) (ntohs((header).uh_ulen))
#define udphdr_check(header) (ntohs((header).uh_sum))
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
