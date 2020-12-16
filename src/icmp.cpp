#include <cstring>
#include <iostream>

#include "utility.hpp"
#include "wire/wire.hpp"
#include "device/unix_socket.hpp"
#include "device/athernet_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 2) { cs120_abort("accept 1 arguments"); }

    std::unique_ptr<BaseSocket> sock = nullptr;

    if (strcmp(argv[1], "-a") == 0) {
        sock = std::unique_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(argv[1], "-e") == 0) {
        sock = std::unique_ptr<BaseSocket>{new AthernetSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    ICMPServer server{std::move(sock), 56789};

    for (size_t i = 0; i < 10; ++i) {
        auto start = std::chrono::steady_clock::now();
        if (server.ping(i, inet_addr("192.168.1.2"), inet_addr("182.61.200.7"), 20000, 30000)) {
            auto end = std::chrono::steady_clock::now();
            std::cout << std::chrono::duration<double, std::milli>(end - start).count()
                      << " ms" << std::endl;
        } else {
            std::cout << "timeout!" << std::endl;
        }

        sleep(1);
    }
}
