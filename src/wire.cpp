#include <cstdio>
#include <unistd.h>

#include "pcap/pcap.h"

#include "wire.hpp"


namespace cs120 {
size_t get_ipv4_total_size(Slice<uint8_t> buffer) {
    const auto *ip_header = buffer.buffer_cast<struct ip>();
    if (ip_header == nullptr) { return 0; }
    size_t size = ip_get_tot_len(*ip_header);
    if (size > buffer.size()) { return 0; }
    return size;
}

const char *bool_to_string(bool value) { return value ? "true" : "false"; }

uint16_t composite_checksum(Slice<uint8_t> buffer_) {
    if (buffer_.size() % 2 != 0) { cs120_abort("length is not multiple of 2!"); }

    Slice<uint16_t> buffer{reinterpret_cast<const uint16_t *>(buffer_.begin()), buffer_.size() / 2};

    uint32_t sum = 0;

    for (auto &item: buffer) { sum += item; }

    return static_cast<uint16_t>(~((sum & 0xffffu) + (sum >> 16u)));
}

void print_mac_addr(const unsigned char (&addr)[ETH_ALEN]) {
    printf("%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

void format(const struct ethhdr &object) {
    printf("Ethernet Header {\n");
    printf("\tsource Address: ");
    print_mac_addr(object.h_source);
    printf(",\n");
    printf("\tdestination Address: ");
    print_mac_addr(object.h_dest);
    printf(",\n");
    printf("\tprotocol: %d,\n", object.h_proto);
    printf("}\n");
}

void format(const struct ip &object) {
    Slice<uint8_t> ip_header{reinterpret_cast<const uint8_t *>(&object), ip_get_ihl(object)};
    const char *checksum = bool_to_string(composite_checksum(ip_header) == 0);

    printf("IP Header {\n");
    printf("\tversion: %d,\n", static_cast<uint32_t>(ip_get_version(object)));
    printf("\tinternet header length: %d,\n", static_cast<uint32_t>(ip_get_ihl(object)));
    printf("\ttype of service: %d,\n", static_cast<uint32_t>(ip_get_tos(object)));
    printf("\ttotal length: %d,\n", ip_get_tot_len(object));
    printf("\tidentification: %d,\n", ip_get_id(object));
    printf("\tdo not fragment: %s,\n", bool_to_string(ip_get_do_not_fragment(object)));
    printf("\tmore fragment: %s,\n", bool_to_string(ip_get_more_fragment(object)));
    printf("\toffset: %d,\n", ip_get_offset(object));
    printf("\ttime to live: %d,\n", static_cast<uint32_t>(ip_get_ttl(object)));
    printf("\tprotocol: %d,\n", static_cast<uint32_t>(ip_get_protocol(object)));
    printf("\theader checksum: %s,\n", checksum);
    printf("\tsource ip: %s,\n", inet_ntoa(ip_get_saddr(object)));
    printf("\tdestination ip: %s,\n", inet_ntoa(ip_get_daddr(object)));
    printf("}\n");
}

void format(const icmp &object) {
    printf("ICMP Header {\n");
    printf("\ttype: %d,\n", object.get_type());
    printf("\tcode: %d,\n", object.get_code());
    printf("\tidentifier: %d,\n", object.get_ident());
    printf("\tsequence: %d,\n", object.get_seq());
//    printf("\tchecksum: %d,\n", udphdr_get_check(object));
    printf("}\n");
}

void format(const udphdr &object) {
    printf("UDP Header {\n");
    printf("\tsource port: %d,\n", udphdr_get_source(object));
    printf("\tdestination port: %d,\n", udphdr_get_dest(object));
    printf("\tlength: %d,\n", udphdr_get_len(object));
//    printf("\tchecksum: %d,\n", udphdr_get_check(object));
    printf("}\n");
}

void format(const Slice<uint8_t> &object) {
    size_t i = 0;

    for (auto &item: object) {
        if (i % 32 == 0) { printf(&"\n%.4X: "[i == 0], static_cast<uint32_t>(i)); }
        printf("%.2X ", item);
        ++i;
    }

    printf("\n");
}

uint32_t get_local_ip() {
    char buffer[1024]{};

    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    int pipe_fd[2] = {-1, -1};

    if (pipe(pipe_fd) != 0) { cs120_abort("pipe error"); }

    int child = fork();

    switch (child) {
        case -1:
            cs120_abort("fork error");
        case 0:
            dup2(pipe_fd[1], STDOUT_FILENO);
            if (execl("/sbin/ifconfig", "/sbin/ifconfig", device->name, nullptr) != 0) {
                cs120_abort("failed to execute ifconfig");
            }
        default:;
    }

    int child_state = 0;

    if (waitpid(child, &child_state, 0) != child) { cs120_abort("waitpid error"); }

    if (!WIFEXITED(child_state)) { cs120_abort("ifconfig execution failed!"); }

    ssize_t size = read(pipe_fd[0], buffer, 1024);

    if (size < 0) { cs120_abort("read error"); }

    const char *start = strstr(buffer, "inet") + sizeof("inet");

    uint32_t ip = inet_addr(start);

    if (ip == static_cast<uint32_t>(-1)) { cs120_abort("invalid ip"); }

    pcap_freealldevs(device);
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    return ip;
}

std::pair<uint32_t, uint16_t> parse_ip_address(const char *str) {
    uint8_t buffer[4]{};
    uint16_t lan_port = 0;

    if (sscanf(str, "%hhd.%hhd.%hhd.%hhd:%hd", &buffer[0], &buffer[1], &buffer[2],
               &buffer[3], &lan_port) != 5) { cs120_abort("input_format_error!"); }

    uint32_t src_ip = *reinterpret_cast<uint32_t *>(buffer);

    return std::make_pair(src_ip, lan_port);
}

void checksum_ip(MutSlice<uint8_t> frame) {
    auto *ip_header = frame.buffer_cast<struct ip>();

    ip_header->ip_sum = 0;
    ip_header->ip_sum = composite_checksum(frame[Range{0, sizeof(struct ip)}]);
}

void generate_ip(MutSlice<uint8_t> frame, uint8_t protocol,
                 uint32_t src_ip, uint32_t dest_ip, size_t len) {
    auto *ip_header = frame.buffer_cast<struct ip>();

    ip_header->ip_hl = sizeof(struct ip) / 4;
    ip_header->ip_v = 4;
    ip_header->ip_tos = 0;
    ip_header->ip_len = htons(sizeof(struct ip) + len);
    ip_header->ip_id = getpid();
    ip_header->ip_off = IP_DF;
    ip_header->ip_ttl = 64;
    ip_header->ip_p = protocol;
    ip_header->ip_src.s_addr = src_ip;
    ip_header->ip_dst.s_addr = dest_ip;

    checksum_ip(frame);
}

void checksum_icmp(MutSlice<uint8_t> frame, size_t icmp_size) {
    auto *icmp_header = frame.buffer_cast<struct icmp>();

    icmp_header->set_sum(0);
    icmp_header->set_sum(composite_checksum(frame[Range{0, icmp_size}]));
}

void generate_icmp(MutSlice<uint8_t> frame, uint32_t src_ip, uint32_t dest_ip,
                   uint8_t type, uint16_t ident, uint16_t seq, Slice<uint8_t> data) {
    size_t icmp_size = sizeof(struct icmp) + data.size();

    generate_ip(frame, IPPROTO_ICMP, src_ip, dest_ip, icmp_size);

    auto icmp_frame = frame[Range{sizeof(struct ip)}];
    auto *icmp_header = icmp_frame.buffer_cast<struct icmp>();

    icmp_header->set_type(type);
    icmp_header->set_code(0);
    icmp_header->set_ident(ident);
    icmp_header->set_seq(seq);

    icmp_frame[Range{sizeof(struct icmp)}][Range{0, data.size()}].copy_from_slice(data);

    checksum_icmp(icmp_frame, icmp_size);
}

void checksum_udp(MutSlice<uint8_t> frame) {
    auto *icmp_header = frame.buffer_cast<struct udphdr>();

    icmp_header->uh_sum = 0;
}

void generate_udp(MutSlice<uint8_t> frame, uint32_t src_ip, uint32_t dest_ip,
                  uint16_t src_port, uint16_t dest_port, Slice<uint8_t> data) {
    size_t udp_size = sizeof(struct udphdr) + data.size();

    generate_ip(frame, IPPROTO_UDP, src_ip, dest_ip, udp_size);

    auto udp_frame = frame[Range{sizeof(struct ip)}];
    auto *udp_header = udp_frame.buffer_cast<struct udphdr>();

    udphdr_set_source(*udp_header, src_port);
    udphdr_set_dest(*udp_header, dest_port);
    udphdr_set_len(*udp_header, data.size());

    udp_frame[Range{sizeof(struct udphdr)}][Range{0, data.size()}].copy_from_slice(data);

    checksum_udp(udp_frame);
}
}
