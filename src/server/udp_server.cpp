#include "server/udp_server.hpp"

#include "wire.hpp"


namespace cs120 {
size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    size_t maximum = udp_max_payload(device->get_mtu());

    while (!data.empty()) {
        auto buffer = device->send();

        size_t size = std::min(maximum, data.size());

        generate_udp(*buffer, src_ip, dest_ip, src_port, dest_port, data[Range{0, size}]);

        data = data[Range{size}];
    }

    return length;
}

size_t UDPServer::recv(MutSlice<uint8_t> data) {
    (void) data;

    cs120_abort("unimplemented!");
}
}
