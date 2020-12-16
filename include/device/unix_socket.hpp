#ifndef CS120_UNIX_SOCKET_HPP
#define CS120_UNIX_SOCKET_HPP


#include <unistd.h>
#include "pthread.h"

#include "utility.hpp"
#include "queue.hpp"
#include "base_socket.hpp"
#include "athernet.hpp"


namespace cs120 {
class UnixSocket : public BaseSocket {
private:
    pthread_t receiver, sender;
    SPSCQueue<PacketBuffer> *receive_queue, *send_queue;
    int athernet;

public:
    explicit UnixSocket(size_t size);

    UnixSocket(UnixSocket &&other) noexcept = default;

    UnixSocket &operator=(UnixSocket &&other) noexcept = default;

    size_t get_mtu() final { return ATHERNET_MTU - 1; }

    SPSCQueueSenderSlotGuard<PacketBuffer> try_send() final { return send_queue->try_send(); }

    SPSCQueueSenderSlotGuard<PacketBuffer> send() final { return send_queue->send(); }

    SPSCQueueReceiverSlotGuard<PacketBuffer> try_recv() final { return receive_queue->try_recv(); }

    SPSCQueueReceiverSlotGuard<PacketBuffer> recv() final { return receive_queue->recv(); }

    ~UnixSocket() override {
        pthread_join(receiver, nullptr);
        pthread_join(sender, nullptr);
        if (athernet != -1) { close(athernet); }
    }
};
}


#endif //CS120_UNIX_SOCKET_HPP
