#ifndef CS120_RAW_SOCKET_HPP
#define CS120_RAW_SOCKET_HPP

#include "pthread.h"

#include "pcap/pcap.h"

#include "utility.hpp"
#include "wire.hpp"
#include "queue.hpp"
#include "mod.hpp"


namespace cs120 {
class RawSocket : public BaseSocket {
private:
    pthread_t receiver;
    SPSCQueue *receive_queue, *send_queue;

public:
    RawSocket(size_t buffer_size, size_t size);

    RawSocket(RawSocket &&other) noexcept = default;

    RawSocket &operator=(RawSocket &&other) noexcept = default;

    SPSCQueueSenderSlotGuard try_send() final { return send_queue->try_send(); }

    SPSCQueueSenderSlotGuard send() final { return send_queue->send(); }

    SPSCQueueReceiverSlotGuard try_recv() final { return receive_queue->try_recv(); }

    SPSCQueueReceiverSlotGuard recv() final { return receive_queue->recv(); }

    ~RawSocket() override = default;
};
}


#endif //CS120_RAW_SOCKET_HPP
