#ifndef CS120_ATHERNET_SOCKET_HPP
#define CS120_ATHERNET_SOCKET_HPP


#include <unistd.h>

#include "utility.hpp"
#include "queue.hpp"
#include "base_socket.hpp"
#include "athernet.hpp"


namespace cs120 {
/// this socket is for mimicking athernet device through unix socket
class AthernetSocket : public BaseSocket {
private:
    pthread_t receiver, sender;
    MPSCReceiver<PacketBuffer> recv_queue;
    MPSCSender<PacketBuffer> send_queue;
    int athernet;

public:
    explicit AthernetSocket(size_t size);

    AthernetSocket(AthernetSocket &&other) noexcept = default;

    AthernetSocket &operator=(AthernetSocket &&other) noexcept = default;

    size_t get_mtu() final { return ATHERNET_MTU - 1; }

    MPSCSenderSlotGuard<PacketBuffer> try_send() final { return send_queue.try_send(); }

    MPSCSenderSlotGuard<PacketBuffer> send() final { return send_queue.send(); }

    MPSCReceiverSlotGuard<PacketBuffer> try_recv() final { return recv_queue.try_recv(); }

    MPSCReceiverSlotGuard<PacketBuffer> recv() final { return recv_queue.recv(); }

    ~AthernetSocket() override {
        pthread_join(receiver, nullptr);
        pthread_join(sender, nullptr);
        if (athernet != -1) { close(athernet); }
    }
};
}


#endif //CS120_ATHERNET_SOCKET_HPP
