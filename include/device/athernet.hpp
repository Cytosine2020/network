#ifndef CS120_ATHERNET_HPP
#define CS120_ATHERNET_HPP


#include <cstdint>

#include "queue.hpp"
#include "wire/wire.hpp"
#include "raw_socket.hpp"


namespace cs120 {
constexpr const char *ATHERNET_SOCKET = "/tmp/athernet.socket";
constexpr size_t ATHERNET_MTU = 256;

struct unix_socket_recv_args {
    int athernet;
    MPSCSender<PacketBuffer> queue;
};

struct unix_socket_send_args {
    int athernet;
    MPSCReceiver<PacketBuffer> queue;
};

void *unix_socket_sender(void *args_);

void *unix_socket_receiver(void *args_);
}


#endif //CS120_ATHERNET_HPP
