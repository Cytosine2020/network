#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "wire/wire.hpp"
#include "device/athernet_socket.hpp"
#include "device/unix_socket.hpp"
#include "server/udp_server.hpp"


using namespace cs120;

int main(int argc, char **argv) {
    if (argc != 6) { cs120_abort("accept 5 arguments"); }

    auto device = argv[1];
    auto src = argv[2];
    auto dest = argv[3];
    auto command = argv[4];
    auto file_name = argv[5];

    auto[src_ip, src_port] = parse_ip_address(src);
    auto[dest_ip, dest_port] = parse_ip_address(dest);

    printf("local %s:%d\n", inet_ntoa(in_addr{src_ip}), src_port);
    printf("remote %s:%d\n", inet_ntoa(in_addr{dest_ip}), dest_port);

    if (strcmp(command, "-s") == 0) {
        int file = open(file_name, O_RDONLY);
        if (file < 0) { cs120_abort("open error"); }

        struct stat tmp{};
        if (fstat(file, &tmp) != 0) { cs120_abort("fstat error"); }

        auto *buffer = mmap(nullptr, tmp.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, file, 0);
        if (buffer == MAP_FAILED) { cs120_abort("mmap error"); }

        close(file);

        printf("sending file `%s`\n", file_name);

        Slice<uint8_t> data{reinterpret_cast<uint8_t *>(buffer), static_cast<size_t>(tmp.st_size)};

        if (strcmp(device, "-a") == 0 || strcmp(device, "-e") == 0) {
            std::shared_ptr<BaseSocket> sock{};

            if (strcmp(device, "-a") == 0) {
                sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
            } else if (strcmp(device, "-e") == 0) {
                sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
            } else {
                cs120_unreachable("");
            }

            UDPServer server{std::move(sock), 64, src_ip, dest_ip, src_port, dest_port};

            sleep(1); // FIXME: the filter is not updated quick enough

            size_t i = 0, j = 0;
            for (; i < data.size(); ++i) {
                if (data[i] == '\n') {
                    server.send(data[Range{j, i + 1}]);
                    j = i + 1;
                    sleep(1);
                }
            }

            if (j < i) { server.send(data[Range{j, i}]); }
        } else if (strcmp(device, "-s") == 0) {
            if (argc < 2) { cs120_abort("need option\n"); }

            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { cs120_abort("socket error"); }

            struct sockaddr_in src_addr{};
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(src_port);
            src_addr.sin_addr = in_addr{htonl(INADDR_ANY)};

            if (bind(sock, reinterpret_cast<const sockaddr *>(&src_addr),
                     sizeof(struct sockaddr_in))) { cs120_abort("send error"); }

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

            close(sock);
        } else {
            cs120_abort("unknown device!");
        }

        munmap(buffer, tmp.st_size);
    } else if (strcmp(command, "-r") == 0) {
        int file = open(file_name, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
        if (file < 0) { cs120_abort("open error"); }

        printf("receiving file `%s`\n", file_name);

        Array<uint8_t> buffer{2048};

        if (strcmp(device, "-a") == 0 || strcmp(device, "-e") == 0) {
            std::shared_ptr<BaseSocket> sock{};

            if (strcmp(device, "-a") == 0) {
                sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
            } else if (strcmp(device, "-e") == 0) {
                sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
            } else {
                cs120_unreachable("");
            }

            UDPServer server{std::move(sock), 64, src_ip, dest_ip, src_port, dest_port};

            for (;;) {
                size_t size = server.recv(buffer[Range{}]);
                if (size == 0) { break; }

                if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
                    cs120_abort("write error");
                }

                printf("%s:%u: ", inet_ntoa(in_addr{dest_ip}), dest_port);
                printf("%.*s\n", static_cast<uint32_t>(size), buffer.begin());
            }
        } else if (strcmp(device, "-s") == 0) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) { cs120_abort("socket error"); }

            struct sockaddr_in src_addr{};
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(src_port);
            src_addr.sin_addr = in_addr{htonl(INADDR_ANY)};

            if (bind(sock, reinterpret_cast<const sockaddr *>(&src_addr),
                     sizeof(struct sockaddr_in))) { cs120_abort("send error"); }

            Array<uint8_t> slice{2048};

            for (;;) {
                struct sockaddr_in dest_addr{};
                socklen_t socklen = sizeof(struct sockaddr_in);

                ssize_t count = recvfrom(sock, slice.begin(), slice.size(), 0,
                                         reinterpret_cast<sockaddr *>(&dest_addr), &socklen);
                if (count == -1) { cs120_abort("recvfrom error"); }
                if (count == 0) { break; }

                if (write(file, slice.begin(), count) != count) { cs120_abort("write error"); }

                printf("%s:%u: ", inet_ntoa(dest_addr.sin_addr), ntohs(dest_addr.sin_port));
                printf("%.*s\n", static_cast<uint32_t>(count), slice.begin());
            }

            close(sock);
        } else {
            cs120_abort("unknown device!");
        }
    } else {
        cs120_abort("unknown command!");
    }
}
