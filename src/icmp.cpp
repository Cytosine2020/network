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

    std::unique_ptr<BaseSocket> sock = nullptr;
    if (strcmp(argv[1], "-a") == 0) {
        sock = std::unique_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(argv[1], "-e") == 0) {
        sock = std::unique_ptr<BaseSocket>{new AthernetSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    auto[src_ip, src_port] = parse_ip_address(argv[2]);
    auto[dest_ip, dest_port] = parse_ip_address(argv[3]);

    printf("local %s:%d\n", inet_ntoa(in_addr{src_ip}), src_port);
    printf("remote %s:%d\n", inet_ntoa(in_addr{dest_ip}), dest_port);

    ICMPServer server{std::move(sock), 56789};

    for (size_t i = 0; i < 10; ++i) {
        auto start = std::chrono::steady_clock::now();
        if (server.ping(i, src_ip, dest_ip, src_port, dest_port)) {
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
