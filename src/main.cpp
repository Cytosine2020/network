#include <cstdio>

#include "wire.hpp"
#include "utility.hpp"

using namespace cs120;

constexpr size_t BUFFER_SIZE = 65536;


int main() {
#if defined(__linux__)
    int raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
#elif defined(__APPLE__)
    int raw_socket = socket(PF_INET, SOCK_RAW, IPPROTO_IP);
    if (raw_socket < 0) { cs120_abort("error in socket"); }

    {
        int yes = 1;
        if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &yes, sizeof(yes)) < 0) {
            cs120_abort("error in setsockopt");
        }
    }
#endif

    auto buffer = Array<uint8_t>{BUFFER_SIZE};

    for (;;) {
        ssize_t buffer_len = recv(raw_socket, buffer.begin(), BUFFER_SIZE, 0);
        if (buffer_len < 0) { cs120_abort("error in recv"); }

#if defined(__linux__)
        auto eth_data = ({
            auto eth_datagram = buffer[Range{0, static_cast<size_t>(buffer_len)}];

            auto *eth = (struct ethhdr *) eth_datagram.begin();

            if (eth->h_proto != 8) { continue; }

            eth_datagram[Range{sizeof(struct ethhdr), eth_datagram.size()}];
        });
#elif defined(__APPLE__)
        auto eth_data = buffer[Range{0, static_cast<size_t>(buffer_len)}];
#endif

        auto *ip = (struct iphdr *) eth_data.begin();

//        if (ip->iphdr_version != 4u) { continue; }

        format(*ip);

        if (ip->iphdr_protocol != 17) { continue; }

        auto ip_data = eth_data[Range{static_cast<size_t>(ip->iphdr_ihl) * 4, eth_data.size()}];
        auto *udp = (struct udphdr *) ip_data.begin();

        format(*udp);

        auto udp_data = ip_data[Range{sizeof(struct udphdr), ip_data.size()}];

        format(udp_data);
    }
}
