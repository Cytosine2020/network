#include "wire/wire.hpp"
#include "device/athernet_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

int main() {
    std::unique_ptr<BaseSocket> sock(new AthernetSocket{64});

    icmp_ping(std::move(sock), inet_addr("192.168.1.2"), inet_addr("182.61.200.7"), 20000);
}
