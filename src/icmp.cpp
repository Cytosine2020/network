#include <cstring>
#include <iostream>

#include "utility.hpp"
#include "wire/wire.hpp"
#include "device/unix_socket.hpp"
#include "device/athernet_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 4) { cs120_abort("accept 3 arguments"); }

    auto device = argv[1];
    auto src = parse_ip_address(argv[2]);
    auto dest = parse_ip_address(argv[3]);

    std::shared_ptr<BaseSocket> sock{};
    if (strcmp(device, "-a") == 0) {
        sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(device, "-e") == 0) {
        sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    printf("local %s:%d\n", inet_ntoa(in_addr{src.ip_addr}), src.port);
    printf("remote %s:%d\n", inet_ntoa(in_addr{dest.ip_addr}), dest.port);

    ICMPServer server{sock, src.ip_addr};
    auto client = server.create_ping(56789, dest.ip_addr, src.port, dest.port);

    sleep(1); // FIXME: the filter is not updated quick enough

    for (size_t i = 0; i < 1000; ++i) {
        auto start = std::chrono::steady_clock::now();
        if (client.ping(i)) {
            auto end = std::chrono::steady_clock::now();
            std::cout << "ping " << i << ": "
                      << std::chrono::duration<double, std::milli>(end - start).count()
                      << " ms" << std::endl;
        } else {
            std::cout << "timeout!" << std::endl;
        }

        sleep(1);
    }
}
