#include "server/icmp_server.hpp"

#include <sys/time.h>
#include <unistd.h>

#include "utility.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"


namespace cs120 {
void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t src_ip, uint32_t dest_ip,
               uint16_t src_port) {
    if (ICMPHeader::max_payload(sock->get_mtu()) < sizeof(struct timeval)) {
        cs120_abort("mtu too small!");
    }

    for (int i = 0; i < 10; i++) {
        struct timeval time{};
        gettimeofday(&time, nullptr);
        Slice<uint8_t> data{reinterpret_cast<uint8_t *>(&time), sizeof(struct timeval)};

        {
            auto buffer = sock->send();
            if (ICMPHeader::generate(*buffer, i + 1, src_ip, dest_ip, ICMPHeader::EchoRequest,
                                     src_port, i, data) == 0) {
                cs120_abort("header generation failed!");
            }

            buffer->buffer_cast<IPV4Header>()->format();
        }

        for (;;) {
            auto buffer = sock->recv();

            auto *ip_header = buffer->buffer_cast<IPV4Header>();
            if (ip_header == nullptr ||
                ip_header->get_protocol() != IPV4Header::ICMP ||
                ip_header->get_destination_ip() != src_ip) { continue; }

            auto ip_data = (*buffer)[Range{ip_header->get_header_length(),
                                           ip_header->get_total_length()}];

            auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
            if (icmp_header == nullptr ||
                icmp_header->get_type() != ICMPHeader::EchoReply ||
                icmp_header->get_code() != 0 ||
                icmp_header->get_identification() != src_port ||
                icmp_header->get_sequence() != i) { continue; }

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

            break;
        }
        sleep(1);
    }
}
}
