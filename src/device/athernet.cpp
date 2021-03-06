#include "device/athernet.hpp"

#include <sys/socket.h>

#include "utility.hpp"
#include "wire/ipv4.hpp"


namespace cs120 {
void *unix_socket_sender(void *args_) {
    auto *args = static_cast<unix_socket_send_args *>(args_);

    Array<uint8_t> mem{ATHERNET_MTU + 3};
    auto buffer = mem[Range{3}];

    for (;;) {
        auto slot = args->queue.recv();
        if (slot.none()) { break; }

        auto *ip_header = slot->buffer_cast<IPV4Header>();
        if (ip_header == nullptr) {
            cs120_warn("invalid package!");
            continue;
        }

        auto size = ip_header->get_total_length();
        if (size >= ATHERNET_MTU) {
            cs120_warn("package truncated!");
            continue;
        }

        buffer[0] = static_cast<uint8_t>(size);
        buffer[Range{1, size + 1}].copy_from_slice((*slot)[Range{0, size}]);

        ssize_t len = send(args->athernet, buffer.begin(), ATHERNET_MTU, 0);
        if (len == 0) { break; }
        if (len != ATHERNET_MTU) { cs120_abort("send error"); }
    }

    delete args;

    return nullptr;
}

void *unix_socket_receiver(void *args_) {
    auto *args = static_cast<unix_socket_recv_args *>(args_);

    Array<uint8_t> mem{ATHERNET_MTU + 3};
    auto buffer = mem[Range{3}];

    for (;;) {
        ssize_t len = recv(args->athernet, buffer.begin(), ATHERNET_MTU, 0);

        if (len == 0) { break; }
        if (len != ATHERNET_MTU) { cs120_abort("recv_line error"); }

        size_t size = buffer[0];
        args->demultiplexer.send(buffer[Range{1, size + 1}]);
        if (args->demultiplexer.is_close()) { break; }
    }

    delete args;

    return nullptr;
}
}
