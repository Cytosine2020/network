#include "device/unix_socket.hpp"

#include <sys/un.h>
#include <sys/socket.h>


const char *ATHERNET_SOCKET = "/tmp/athernet.socket";


namespace cs120 {
UnixSocket::UnixSocket(size_t buffer_size, size_t size) :
        receive_queue{nullptr}, send_queue{nullptr}, athernet{-1} {
    receive_queue = new SPSCQueue{buffer_size, size};
    send_queue = new SPSCQueue{buffer_size, size};

    athernet = socket(AF_UNIX, SOCK_STREAM, 0);

    if (athernet == -1) { cs120_abort("client socket error"); }

//    struct sockaddr_un cli_un{};
//    cli_un.sun_family = AF_UNIX;
//    strcpy(cli_un.sun_path, ATHERNET_SOCKET);
//    len = offsetof(struct sockaddr_un, sun_path) + strlen(cli_un.sun_path);
//    unlink(cli_un.sun_path);
//    if (bind(sock_flag, (struct sockaddr *) &cli_un, len) < 0) {
//        cs120_abort("bind error");
//    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ATHERNET_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(athernet, reinterpret_cast<struct sockaddr *>(&addr), sizeof(sockaddr_un)) != 0) {
        cs120_abort("connect error");
    }
}

SPSCQueueSenderSlotGuard UnixSocket::send() { cs120_abort("unimplemented!"); }

SPSCQueueReceiverSlotGuard UnixSocket::recv() { cs120_abort("unimplemented!"); }
}
