#include "device/athernet.hpp"

#include <sys/socket.h>

#include "utility.hpp"
#include "wire/ipv4.hpp"


namespace cs120 {
void *unix_socket_sender(void *args_) {
    auto *args = static_cast<unix_socket_send_args *>(args_);

    Array<uint8_t> buffer{ATHERNET_MTU};

    for (;;) {
        auto slot = args->queue.recv();
        auto *ip_header = slot->buffer_cast<IPV4Header>();
        if (ip_header == nullptr) {
            cs120_warn("invalid package!");
            continue;
        }

        auto ip_header_size = ip_header->get_total_length();

        if (ip_header_size >= ATHERNET_MTU) {
            cs120_warn("package truncated!");
            continue;
        }

        buffer[0] = static_cast<uint8_t>(ip_header_size);
        buffer[Range{1, ip_header_size + 1}].copy_from_slice((*slot)[Range{0, ip_header_size}]);

        ssize_t len = send(args->athernet, buffer.begin(), ATHERNET_MTU, 0);
        if (len == 0) { return nullptr; }
        if (len != ATHERNET_MTU) { cs120_abort("send error"); }
    }
}

void *unix_socket_receiver(void *args_) {
    auto *args = static_cast<unix_socket_recv_args *>(args_);

    Array<uint8_t> buffer{ATHERNET_MTU};

    for (;;) {
        ssize_t len = recv(args->athernet, buffer.begin(), ATHERNET_MTU, 0);
        if (len == 0) { return nullptr; }
        if (len != ATHERNET_MTU) { cs120_abort("recv error"); }

        auto slot = args->queue.try_send();
        if (slot->empty()) {
            cs120_warn("package loss!");
        } else {
            size_t size = buffer[0];
            (*slot)[Range{0, size}].copy_from_slice(buffer[Range{1, size + 1}]);
        }
    }
}
}
