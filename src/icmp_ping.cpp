#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

static constexpr size_t SOCKET_BUFFER_SIZE = 256;

int main() {
    std::unique_ptr<BaseSocket> sock(new UnixSocket{SOCKET_BUFFER_SIZE, 64});

    icmp_ping(std::move(sock), inet_addr("182.61.200.7"));
}
