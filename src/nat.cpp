#include "wire.hpp"
#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"
#include "server/nat_server.hpp"


using namespace cs120;

int main() {
    uint32_t ip_addr = get_local_ip();

    std::unique_ptr<BaseSocket> lan(new UnixSocket{64});
    std::unique_ptr<BaseSocket> wan(new RawSocket{64, ip_addr});

    NatServer server{ip_addr, std::move(lan), std::move(wan), {}};
}
