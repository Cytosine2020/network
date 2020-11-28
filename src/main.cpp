#include "device/raw_socket.hpp"

using namespace cs120;


int main() {
    RawSocket raw_socket{};

    for (;;) {
        auto buffer = raw_socket.recv();

        auto *ip_header = buffer->buffer_cast<struct iphdr>();

        auto ip_datagram = (*buffer)[Range{0, iphdr_get_tot_len(*ip_header)}];

        if (iphdr_get_version(*ip_header) != 4u) { continue; }

        if (iphdr_get_protocol(*ip_header) != 17) { continue; }

        format(*ip_header);

        auto ip_data = ip_datagram[Range{static_cast<size_t>(iphdr_get_ihl(*ip_header)) * 4}];
        auto *udp = ip_data.buffer_cast<struct udphdr>();

        format(*udp);

        auto udp_data = ip_data[Range{sizeof(struct udphdr)}];

        format(udp_data);
    }
}
