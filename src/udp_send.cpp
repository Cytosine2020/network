#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "wire.hpp"
#include "device/athernet_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 2) { cs120_abort("accept 2 arguments"); }

    int file = open(argv[1], O_RDONLY);

    struct stat tmp{};

    if (fstat(file, &tmp) != 0) { cs120_abort("fstat error"); }

    auto *buffer = reinterpret_cast<uint8_t *>(mmap(nullptr, tmp.st_size, PROT_READ,
                                                    MAP_FILE | MAP_PRIVATE, file, 0));
    if (buffer == MAP_FAILED) { cs120_abort("mmap error"); }

    close(file);

    std::unique_ptr<BaseSocket> sock(new AthernetSocket{64});

    printf("sending file `%s`, size %ld\n", argv[1], tmp.st_size);

    UDPServer server{std::move(sock), inet_addr("192.168.1.2"), inet_addr("0.0.0.0"),
                     20000, 20000};

    server.send(Slice<uint8_t>{buffer, static_cast<size_t>(tmp.st_size)});

    sleep(1);

    munmap(buffer, tmp.st_size);
}
