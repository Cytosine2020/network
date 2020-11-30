#include <fcntl.h>

#include "wire.hpp"
#include "device/unix_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 3) { cs120_abort("accept 3 arguments"); }

    auto[dest_ip, dest_port] = parse_ip_address(argv[1]);

    int file = open(argv[1], O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (file < 0) { cs120_abort("open error"); }

    std::unique_ptr<BaseSocket> sock(new UnixSocket{64});

    printf("receiving file `%s`, from %s:%d\n", argv[1], inet_ntoa(in_addr{dest_ip}), dest_port);

    UDPServer server{std::move(sock), inet_addr("192.168.1.2"), dest_ip, 20000, dest_port};

    Array<uint8_t> buffer{2048};

    for (;;) {
        size_t size = server.recv(buffer);

        if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
            cs120_abort("write error");
        }

        format(buffer[Range{0, size}]);
    }
}
