#include "server/icmp_server.hpp"

#include <sys/time.h>
#include <unistd.h>

#include "utility.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"


namespace cs120 {
void send_ping(std::unique_ptr<BaseSocket> &sock, size_t ident, uint32_t src_ip, uint32_t dest_ip,
               uint16_t src_port) {
    struct timeval time{};
    gettimeofday(&time, nullptr);
    Slice<uint8_t> data{reinterpret_cast<uint8_t *>(&time), sizeof(struct timeval)};

    auto buffer = sock->send();
    if (ICMPHeader::generate(*buffer, ident + 1, src_ip, dest_ip, ICMPType::EchoRequest,
                             src_port, ident, data) == 0) {
        cs120_abort("header generation failed!");
    }
}

void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t src_ip, uint32_t dest_ip,
               uint16_t src_port) {
    if (ICMPHeader::max_payload(sock->get_mtu()) < sizeof(struct timeval)) {
        cs120_abort("mtu too small!");
    }

    size_t i = 0;

    send_ping(sock, i++, src_ip, dest_ip, src_port);

    for (;;) {
        auto buffer = sock->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split(*buffer);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        if (ip_header->get_protocol() != IPV4Protocol::ICMP ||
                ip_header->get_dest_ip() != src_ip) { continue; }

        auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        switch (icmp_header->get_type()) {
            case ICMPType::EchoReply: {
                if (icmp_header->get_code() != 0 ||
                    icmp_header->get_identification() != src_port ||
                    icmp_header->get_sequence() != i - 1) { continue; }

                struct timeval end{};
                gettimeofday(&end, nullptr);

                auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
                if (icmp_data.size() != sizeof(struct timeval)) { continue; }
                auto begin = reinterpret_cast<struct timeval *>(icmp_data.begin());

                uint32_t interval = (end.tv_usec - begin->tv_usec) / 1000 +
                                    (end.tv_sec - begin->tv_sec) * 1000;

                ip_header->format();
                icmp_header->format();

                printf("TTL %d ms\n", interval);

                send_ping(sock, i++, src_ip, dest_ip, src_port);
                break;
            }
            case ICMPType::EchoRequest: {
                if (icmp_header->get_code() != 0) { continue; }
                icmp_header->set_type(ICMPType::EchoReply);
                auto range = Range{0, ip_header->get_total_length()};
                (*sock->send())[range].copy_from_slice((*buffer)[range]);
            }
                break;
            default:
                break;
        }
    }
}
}
