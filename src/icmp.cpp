#include <cstring>
#include <iostream>

#include "utility.hpp"
#include "wire/wire.hpp"
#include "device/unix_socket.hpp"
#include "device/athernet_socket.hpp"
#include "device/raw_socket.hpp"
#include "server/icmp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc < 3) { cs120_abort("accept 3 arguments"); }

    auto device = argv[1];
    auto local = parse_ip_address(argv[2]);

    std::shared_ptr<BaseSocket> sock{};
    if (strcmp(device, "-a") == 0) {
        sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(device, "-e") == 0) {
        sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
    } else if (strcmp(device, "-r") == 0) {
        sock = std::shared_ptr<BaseSocket>{new RawSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    ICMPServer server{sock, local.ip_addr};

    sleep(1); // FIXME: the filter is not updated quick enough

    if (argc >= 5) {
        auto command = argv[3];
        auto remote = parse_ip_address(argv[4]);

        printf("local %s:%d\n", inet_ntoa(in_addr{local.ip_addr}), local.port);
        printf("remote %s:%d\n", inet_ntoa(in_addr{remote.ip_addr}), remote.port);

        auto client = server.create_ping(56789, remote.ip_addr, local.port, remote.port);

        if (strcmp(command, "-p") == 0) {
            for (size_t i = 0; i < std::numeric_limits<size_t>::max(); ++i) {
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
    }
}
