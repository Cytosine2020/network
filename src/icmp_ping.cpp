#include "wire.hpp"
#include "device/raw_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

int main() {
    std::unique_ptr<BaseSocket> sock(new RawSocket{64});

    icmp_ping(std::move(sock), get_local_ip(), inet_addr("182.61.200.7"), 20000);
}
