#include "wire/wire.hpp"
#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"
#include "server/nat_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    uint32_t ip_addr = get_local_ip();

    Array<std::pair<uint32_t, uint16_t>> ip_map{static_cast<size_t>(argc - 1)};

    for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
        ip_map[i - 1] = parse_ip_address(argv[i]);
    }

    std::unique_ptr<BaseSocket> lan(new UnixSocket{64});
    std::unique_ptr<BaseSocket> wan(new RawSocket{64, ip_addr});

    NatServer server{ip_addr, std::move(lan), std::move(wan), ip_map};
}
