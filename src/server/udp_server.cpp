#include "server/udp_server.hpp"


namespace cs120 {
size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    while (!data.empty()) {

    }

    return length;
}

size_t UDPServer::recv(MutSlice<uint8_t> data) {
    cs120_abort("unimplemented!");
}
}
