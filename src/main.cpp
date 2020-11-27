#include <unistd.h>

#include "device/raw_socket.hpp"

using namespace cs120;


int main() {
    RawSocket raw_socket{};

    for (;;) {
        auto packet = raw_socket.recv();

        Slice<uint8_t> eth_datagram{packet.data(), packet.size()};

        auto *eth = (struct ethhdr *) eth_datagram.begin();

        if (eth->h_proto != 8) { continue; }

        auto eth_data = eth_datagram[Range{sizeof(struct ethhdr)}];

        auto *ip = (struct iphdr *) eth_data.begin();

        if (ip->iphdr_version != 4u) { continue; }

        format(*ip);

        if (ip->iphdr_protocol != 17) { continue; }

        auto ip_data = eth_data[Range{static_cast<size_t>(ip->iphdr_ihl) * 4}];
        auto *udp = (struct udphdr *) ip_data.begin();

        format(*udp);

        auto udp_data = ip_data[Range{sizeof(struct udphdr)}];

        format(udp_data);
    }
}
