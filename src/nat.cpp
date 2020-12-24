#include "wire/wire.hpp"
#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"
#include "server/nat_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc < 2) { cs120_abort("accept 1 or more arguments"); }

    uint16_t lan_ip = inet_addr(argv[1]);
    uint32_t wan_addr = get_local_ip();

    Array<EndPoint> ip_map{static_cast<size_t>(argc - 2)};

    for (size_t i = 2; i < static_cast<size_t>(argc); ++i) {
        ip_map[i - 2] = parse_ip_address(argv[i]);
    }

    std::shared_ptr<BaseSocket> lan{new UnixSocket{64}};
    std::shared_ptr<BaseSocket> wan{new RawSocket{64}};

    NatServer server{lan_ip, wan_addr, std::move(lan), std::move(wan), 64, ip_map};
}
