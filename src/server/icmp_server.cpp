#include <sys/time.h>

#include "server/icmp_server.hpp"

#include "wire.hpp"


namespace cs120 {
void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t dest) {
    uint32_t src = get_local_ip();

    for (int i = 0; i < 10; i++) {
        {
            auto buffer = sock->send();
            Array<uint8_t> times = Array<uint8_t>(sizeof(struct timeval));
            auto *start = times.buffer_cast<struct timeval>();
            gettimeofday(start, nullptr);
            generate_icmp_request(*buffer, src, dest, i, times);
        }

        for (;;) {
            auto buffer = sock->recv();

            auto *ip_header = buffer->buffer_cast<struct ip>();

            auto ip_datagram = (*buffer)[Range{0, ip_get_tot_len(*ip_header)}];

            if (ip_get_protocol(*ip_header) != 1) { continue; }

            auto ip_data = ip_datagram[Range{static_cast<size_t>(ip_get_ihl(*ip_header))}];
            auto *icmp = ip_data.buffer_cast<struct icmp>();
            if (icmp->type != 0) { continue; }

            struct timeval tv1{};
            gettimeofday(&tv1, nullptr);

            auto udp_data = ip_data[Range{sizeof(struct icmp)}].buffer_cast<struct timeval>();

            format(*ip_header);

            uint32_t interval = (tv1.tv_usec - udp_data->tv_usec) / 1000 +
                                (tv1.tv_sec - udp_data->tv_sec) * 1000;

            printf("TTL %d ms\n", interval);

            break;
        }
    }
}
}
