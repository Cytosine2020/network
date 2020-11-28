#include "device/unix_socket.hpp"

#include <sys/un.h>
#include <sys/socket.h>


const char *ATHERNET_SOCKET = "/tmp/athernet.socket";


namespace {
using namespace cs120;

struct unix_socket_args {
    int athernet;
    SPSCQueue *queue;
};

void *unix_socket_sender(void *args_) {
    for (;;) {
        auto *args = static_cast<unix_socket_args *>(args_);

        auto slot = args->queue->recv();

        ssize_t size = send(args->athernet, slot->begin(), slot->size(), 0);
        if (size < 0) { cs120_abort("send error"); }
    }

    return nullptr;
}

void *unix_socket_receiver(void *args_) {
    for (;;) {
        auto *args = static_cast<unix_socket_args *>(args_);

        auto slot = args->queue->send();

        ssize_t size = recv(args->athernet, slot->begin(), slot->size(), 0);
        if (size < 0) { cs120_abort("recv error"); }
    }

    return nullptr;
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

    unix_socket_args receiver_args {
        .athernet = athernet,
        .queue = receive_queue,
    };

    unix_socket_args sender_args {
            .athernet = athernet,
            .queue = send_queue,
    };

    pthread_create(&receiver, nullptr, unix_socket_receiver, &receiver_args);
    pthread_create(&sender, nullptr, unix_socket_sender, &sender_args);
}

SPSCQueueSenderSlotGuard UnixSocket::send() { return send_queue->try_send(); }

SPSCQueueReceiverSlotGuard UnixSocket::recv() { return receive_queue->recv(); }
}
