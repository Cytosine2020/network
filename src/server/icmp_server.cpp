#include "server/icmp_server.hpp"

#include <sys/time.h>
#include <unistd.h>

#include "wire.hpp"


namespace cs120 {
void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t src_ip, uint32_t dest_ip,
               uint16_t src_port) {
    if (icmp_max_payload(sock->get_mtu()) < sizeof(struct timeval)) {
        cs120_abort("mtu too small!");
    }

    for (int i = 0; i < 10; i++) {
        struct timeval time{};
        gettimeofday(&time, nullptr);
        Slice<uint8_t> data{reinterpret_cast<uint8_t *>(&time), sizeof(struct timeval)};

        {
            auto buffer = sock->send();
            generate_icmp(*buffer, src_ip, dest_ip, 8, src_port, i, data);
        }

        for (;;) {
            auto buffer = sock->recv();

            auto *ip_header = buffer->buffer_cast<struct ip>();
            auto ip_datagram = (*buffer)[Range{0, ip_get_tot_len(*ip_header)}];

            if (ip_get_version(*ip_header) != 4 ||
                ip_get_protocol(*ip_header) != 1 ||
                ip_get_daddr(*ip_header).s_addr != src_ip) { continue; }

            auto ip_data = ip_datagram[Range{static_cast<size_t>(ip_get_ihl(*ip_header))}];
            auto *icmp_header = ip_data.buffer_cast<icmp>();

            if (icmp_header->get_type() != 0 ||
                icmp_header->get_code() != 0 ||
                icmp_header->get_ident() != src_port ||
                icmp_header->get_seq() != i) { continue; }

            struct timeval tv1{};
            gettimeofday(&tv1, nullptr);

            auto udp_data = ip_data[Range{sizeof(icmp)}].buffer_cast<struct timeval>();

            format(*ip_header);
            format(*icmp_header);

            uint32_t interval = (tv1.tv_usec - udp_data->tv_usec) / 1000 +
                                (tv1.tv_sec - udp_data->tv_sec) * 1000;

            printf("TTL %d ms\n", interval);

            break;
        }
        sleep(1);
    }
}
}
