#include "athernet.hpp"

#include <sys/socket.h>

#include "utility.hpp"


namespace cs120 {
void *unix_socket_sender(void *args_) {
    uint8_t buffer[ATHERNET_MTU]{};

    for (;;) {
        auto *args = static_cast<unix_socket_args *>(args_);

        auto slot = args->queue->recv();

        auto size = get_ipv4_data_size(*slot);

        if (size == 0) { cs120_abort("invalid package!"); }
        if (size >= ATHERNET_MTU) { cs120_abort("package truncated!"); }

        buffer[0] = static_cast<uint8_t>(size);
        MutSlice<uint8_t>{buffer, ATHERNET_MTU}[Range{1}][Range{0, size}]
                .copy_from_slice((*slot)[Range{0, size}]);

        if (send(args->athernet, buffer, ATHERNET_MTU, 0) != ATHERNET_MTU) {
            cs120_abort("send error");
        }
    }
}

void *unix_socket_receiver(void *args_) {
    uint8_t buffer[ATHERNET_MTU]{};

    for (;;) {
        auto *args = static_cast<unix_socket_args *>(args_);

        auto slot = args->queue->try_send();

        if (slot->empty()) {
            cs120_warn("package loss!");
        } else {
            if (recv(args->athernet, buffer, ATHERNET_MTU, 0) != ATHERNET_MTU) {
                cs120_abort("recv error");
            }

            size_t size = buffer[0];

            (*slot)[Range{0, size}].copy_from_slice(
                    MutSlice<uint8_t>{buffer, ATHERNET_MTU}[Range{1}][Range{0, size}]
            );
        }
    }
}
}
