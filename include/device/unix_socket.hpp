#ifndef CS120_UNIX_SOCKET_HPP
#define CS120_UNIX_SOCKET_HPP


#include <unistd.h>

#include "utility.hpp"
#include "queue.hpp"
#include "mod.hpp"


namespace cs120 {
class UnixSocket : public BaseSocket {
private:
    pthread_t receiver, sender;
    SPSCQueue *receive_queue, *send_queue;
    int athernet;

public:
    UnixSocket(size_t buffer_size, size_t size);

    UnixSocket(UnixSocket &&other) noexcept = default;

    UnixSocket &operator=(UnixSocket &&other) noexcept = default;

    SPSCQueueSenderSlotGuard send() final;

    SPSCQueueReceiverSlotGuard recv() final;

    ~UnixSocket() override {
        if (athernet != -1) { close(athernet); }
    }
};
}


#endif //CS120_UNIX_SOCKET_HPP
