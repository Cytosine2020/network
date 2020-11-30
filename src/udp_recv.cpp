#include <fcntl.h>

#include "wire.hpp"
#include "device/raw_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    uint32_t ip_addr = get_local_ip();

    if (argc != 2) { cs120_abort("accept 2 arguments"); }

    int file = open(argv[1], O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (file < 0) { cs120_abort("open error"); }

    std::unique_ptr<BaseSocket> sock(new RawSocket{64, ip_addr});

    printf("receiving file `%s`\n", argv[1]);

    UDPServer server{std::move(sock), ip_addr, ip_addr, 20000, 20001};

    Array<uint8_t> buffer{2048};

    size_t size = server.recv(buffer);

    if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
        cs120_abort("write error");
    }
}
