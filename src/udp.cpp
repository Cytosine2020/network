#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "wire/wire.hpp"
#include "device/raw_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 5) { cs120_abort("accept 4 arguments"); }

    auto src_ip = get_local_ip();
    uint16_t src_port = 20000;
    auto[dest_ip, dest_port] = parse_ip_address(argv[3]);

    printf("bind %s:%d\n", inet_ntoa(in_addr{src_ip}), src_port);
    printf("listen %s:%d\n", inet_ntoa(in_addr{dest_ip}), dest_port);

    if (strcmp(argv[1], "-s") == 0) {
        int file = open(argv[4], O_RDONLY);
        if (file < 0) { cs120_abort("open error"); }

        struct stat tmp{};
        if (fstat(file, &tmp) != 0) { cs120_abort("fstat error"); }

        auto *buffer = mmap(nullptr, tmp.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, file, 0);
        if (buffer == MAP_FAILED) { cs120_abort("mmap error"); }

        close(file);

        printf("sending file `%s`\n", argv[4]);

        Slice<uint8_t> data{reinterpret_cast<uint8_t *>(buffer), static_cast<size_t>(tmp.st_size)};

        if (strcmp(argv[2], "-a") == 0) {
            std::unique_ptr<BaseSocket> sock(new RawSocket{64, get_local_ip()});
            UDPServer server{std::move(sock), src_ip, dest_ip, src_port, dest_port};

            size_t i = 0, j = 0;
            for (; i < data.size(); ++i) {
                if (data[i] == '\n') {
                    server.send(data[Range{j, i + 1}]);
                    j = i + 1;
                    sleep(1);
                }
            }

            if (j < i) { server.send(data[Range{j, i}]); }
        } else if (strcmp(argv[2], "-e") == 0) {
            if (argc < 2) { cs120_abort("need option\n"); }

            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { cs120_abort("socket error"); }

            struct sockaddr_in src_addr{};
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(20000);
            src_addr.sin_addr = in_addr{htonl(INADDR_ANY)};

            if (bind(sock, reinterpret_cast<const sockaddr *>(&src_addr),
                     sizeof(struct sockaddr_in))) { cs120_abort("bind error"); }

            struct sockaddr_in dest_addr{};
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(dest_port);
            dest_addr.sin_addr = in_addr{dest_ip};

            size_t i = 0, j = 0;
            for (; i < data.size(); ++i) {
                if (data[i] == '\n') {
                    auto slice = data[Range{j, i + 1}];

                    if (sendto(sock, slice.begin(), slice.size(), 0,
                               reinterpret_cast<const sockaddr *>(&dest_addr),
                               sizeof(struct sockaddr_in)) == -1) { cs120_abort("sendto error"); }

                    j = i + 1;
                    sleep(1);
                }
            }

            if (j < i) {
                auto slice = data[Range{j, i + 1}];

                if (sendto(sock, slice.begin(), slice.size(), 0,
                           reinterpret_cast<const sockaddr *>(&dest_addr),
                           sizeof(struct sockaddr_in)) == -1) { cs120_abort("sendto error"); }
            }
        }

        munmap(buffer, tmp.st_size);
    } else if (strcmp(argv[1], "-r") == 0) {
        int file = open(argv[4], O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
        if (file < 0) { cs120_abort("open error"); }

        printf("receiving file `%s`\n", argv[4]);

        Array<uint8_t> buffer{2048};

        if (strcmp(argv[2], "-a") == 0) {
            std::unique_ptr<BaseSocket> sock(new RawSocket{64, get_local_ip()});
            UDPServer server{std::move(sock), src_ip, dest_ip, src_port, dest_port};

            for (;;) {
                size_t size = server.recv(buffer);

                if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
                    cs120_abort("write error");
                }

                printf("%s:%u: ", inet_ntoa(in_addr{dest_ip}), dest_port);
                printf("%.*s\n", static_cast<uint32_t>(size), buffer.begin());
            }
        } else if (strcmp(argv[2], "-e") == 0) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { cs120_abort("socket error"); }

            struct sockaddr_in src_addr{};
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(20000);
            src_addr.sin_addr = in_addr{htonl(INADDR_ANY)};

            if (bind(sock, reinterpret_cast<const sockaddr *>(&src_addr),
                     sizeof(struct sockaddr_in))) { cs120_abort("bind error"); }

            Array<uint8_t> slice{2048};

            for (;;) {
                struct sockaddr_in dest_addr{};
                socklen_t socklen = sizeof(struct sockaddr_in);

                ssize_t count = recvfrom(sock, slice.begin(), slice.size(), 0,
                                         reinterpret_cast<sockaddr *>(&dest_addr), &socklen);
                if (count == -1) { cs120_abort("recvfrom error"); }

                if (write(file, slice.begin(), count) != count) { cs120_abort("write error"); }

                printf("%s:%u: ", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
                printf("%.*s\n", static_cast<uint32_t>(count), slice.begin());
            }
        }
    } else {
        cs120_abort("unknown command!");
    }
}
