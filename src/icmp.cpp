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
    auto src = argv[2];
    auto dest = argv[3];

    std::shared_ptr<BaseSocket> sock = nullptr;
    if (strcmp(device, "-a") == 0) {
        sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(device, "-e") == 0) {
        sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    auto[src_ip, src_port] = parse_ip_address(src);
    auto[dest_ip, dest_port] = parse_ip_address(dest);

    printf("local %s:%d\n", inet_ntoa(in_addr{src_ip}), src_port);
    printf("remote %s:%d\n", inet_ntoa(in_addr{dest_ip}), dest_port);

    ICMPServer server{std::move(sock), src_ip};
    auto client = server.create_ping(56789, dest_ip, src_port, dest_port);

    sleep(1); // FIXME: the filter is not updated quick enough

    for (size_t i = 0; i < 10; ++i) {
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
