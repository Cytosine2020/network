#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "wire/wire.hpp"
#include "device/unix_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 3) { cs120_abort("accept 3 arguments"); }

    auto[dest_ip, dest_port] = parse_ip_address(argv[1]);

    int file = open(argv[2], O_RDONLY);
    if (file < 0) { cs120_abort("open error"); }

    struct stat tmp{};

    if (fstat(file, &tmp) != 0) { cs120_abort("fstat error"); }

    auto *buffer = reinterpret_cast<uint8_t *>(mmap(nullptr, tmp.st_size, PROT_READ,
                                                    MAP_FILE | MAP_PRIVATE, file, 0));
    if (buffer == MAP_FAILED) { cs120_abort("mmap error"); }

    close(file);

    std::unique_ptr<BaseSocket> sock(new UnixSocket{64});

    printf("sending file `%s`, size %d, to %s:%d\n", argv[2], static_cast<int32_t>(tmp.st_size),
           inet_ntoa(in_addr{dest_ip}), dest_port);

    UDPServer server{std::move(sock), inet_addr("192.168.1.2"), dest_ip, 20000, dest_port};

    Slice<uint8_t> data{buffer, static_cast<size_t>(tmp.st_size)};

    size_t i = 0, j = 0;
    for (; i < data.size(); ++i) {
        if (data[i] == '\n') {
            server.send(data[Range{j, i}]);
            j = i;
        }
    }

    if (j < i) {
        server.send(data[Range{j, i}]);
    }

    munmap(buffer, tmp.st_size);
}
