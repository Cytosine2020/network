#include "device/unix_socket.hpp"

#include <sys/un.h>
#include <sys/socket.h>

#include "utility.hpp"
#include "device/athernet.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
UnixSocket::UnixSocket(size_t size) :
        receiver{}, sender{}, recv_queue{nullptr}, send_queue{nullptr}, athernet{-1} {
    auto[recv_sender, recv_receiver] = MPSCQueue<PacketBuffer>::channel(size);
    auto[send_sender, send_receiver] = MPSCQueue<PacketBuffer>::channel(size);

    recv_queue = std::move(recv_receiver);
    send_queue = std::move(send_sender);

    athernet = socket(AF_UNIX, SOCK_STREAM, 0);

    if (athernet == -1) { cs120_abort("client socket error"); }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ATHERNET_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(athernet, reinterpret_cast<struct sockaddr *>(&addr), sizeof(sockaddr_un)) != 0) {
        cs120_abort("connect error");
    }

    if (unlink(ATHERNET_SOCKET) != 0) { cs120_abort("unlink error"); }

    auto *receiver_args = new unix_socket_recv_args{
            .athernet = athernet,
            .queue = std::move(recv_sender),
    };

    auto *sender_args = new unix_socket_send_args{
            .athernet = athernet,
            .queue = std::move(send_receiver),
    };

    pthread_create(&receiver, nullptr, unix_socket_receiver, receiver_args);
    pthread_create(&sender, nullptr, unix_socket_sender, sender_args);
}
}
