#include "device/unix_socket.hpp"

#include <sys/un.h>
#include <sys/socket.h>

#include "wire.hpp"


const char *ATHERNET_SOCKET = "/tmp/athernet.socket";
const size_t ATHERNET_MTU = 256;


namespace {
using namespace cs120;

struct unix_socket_args {
    int athernet;
    SPSCQueue *queue;
};

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


namespace cs120 {
UnixSocket::UnixSocket(size_t buffer_size, size_t size) :
        receiver{}, sender{}, receive_queue{nullptr}, send_queue{nullptr}, athernet{-1} {
    receive_queue = new SPSCQueue{buffer_size, size};
    send_queue = new SPSCQueue{buffer_size, size};

    athernet = socket(AF_UNIX, SOCK_STREAM, 0);

    if (athernet == -1) { cs120_abort("client socket error"); }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ATHERNET_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(athernet, reinterpret_cast<struct sockaddr *>(&addr), sizeof(sockaddr_un)) != 0) {
        cs120_abort("connect error");
    }

    if (unlink(ATHERNET_SOCKET) != 0) { cs120_abort("unlink error"); }

    unix_socket_args *receiver_args = new unix_socket_args{
            .athernet = athernet,
            .queue = receive_queue,
    };

    unix_socket_args *sender_args = new unix_socket_args{
            .athernet = athernet,
            .queue = send_queue,
    };

    pthread_create(&receiver, nullptr, unix_socket_receiver, receiver_args);
    pthread_create(&sender, nullptr, unix_socket_sender, sender_args);
}
}
