#ifndef CS120_RAW_SOCKET_HPP
#define CS120_RAW_SOCKET_HPP

#include <vector>
#include "pthread.h"

#include "pcap/pcap.h"

#include "utility.hpp"
#include "wire.hpp"
#include "queue.hpp"
#include "mod.hpp"


namespace cs120 {
static constexpr size_t SOCKET_BUFFER_SIZE = 2048;

class RawSocket : public BaseSocket {
private:
    pthread_t receiver;
    SPSCQueue *receive_queue;

public:
    RawSocket();

    RawSocket(RawSocket &&other) noexcept = default;

    RawSocket &operator=(RawSocket &&other) noexcept = default;

    SPSCQueueSenderSlotGuard send() final;

    SPSCQueueReceiverSlotGuard recv() final;

    ~RawSocket() override = default;
};
}


#endif //CS120_RAW_SOCKET_HPP
