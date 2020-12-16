#include "device/unix_socket.hpp"

#include <sys/un.h>
#include <sys/socket.h>

#include "utility.hpp"
#include "device/athernet.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
UnixSocket::UnixSocket(size_t size) :
        receiver{}, sender{}, receive_queue{nullptr}, send_queue{nullptr}, athernet{-1} {
    receive_queue = new SPSCQueue<PacketBuffer>{size};
    send_queue = new SPSCQueue<PacketBuffer>{size};

    athernet = socket(AF_UNIX, SOCK_STREAM, 0);

    if (athernet == -1) { cs120_abort("client socket error"); }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ATHERNET_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(athernet, reinterpret_cast<struct sockaddr *>(&addr), sizeof(sockaddr_un)) != 0) {
        cs120_abort("connect error");
    }

    if (unlink(ATHERNET_SOCKET) != 0) { cs120_abort("unlink error"); }

    auto *receiver_args = new unix_socket_args{
            .athernet = athernet,
            .queue = receive_queue,
    };

    auto *sender_args = new unix_socket_args{
            .athernet = athernet,
            .queue = send_queue,
    };

    pthread_create(&receiver, nullptr, unix_socket_receiver, receiver_args);
    pthread_create(&sender, nullptr, unix_socket_sender, sender_args);
}
}
