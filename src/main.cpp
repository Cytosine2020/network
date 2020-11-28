#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"

using namespace cs120;

static constexpr size_t SOCKET_BUFFER_SIZE = 2048;


int main() {
//    UnixSocket unix_socket{SOCKET_BUFFER_SIZE, 16};
    RawSocket raw_socket{SOCKET_BUFFER_SIZE, 16};
    for (int i=0;i<10;i++){
        struct ip *ip_header=new ip();

    }
    for (;;) {
        auto buffer = raw_socket.recv();

        auto *ip_header = buffer->buffer_cast<struct ip>();

        auto ip_datagram = (*buffer)[Range{0, ip_get_tot_len(*ip_header)}];

        if (ip_get_version(*ip_header) != 4u) { continue; }

        if (ip_get_protocol(*ip_header) != 17) { continue; }

        format(*ip_header);

        auto ip_data = ip_datagram[Range{static_cast<size_t>(ip_get_ihl(*ip_header))}];
        auto *udp = ip_data.buffer_cast<struct udphdr>();

        format(*udp);

        auto udp_data = ip_data[Range{sizeof(struct udphdr)}];

        format(udp_data);
    }
}
