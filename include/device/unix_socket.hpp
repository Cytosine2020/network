#ifndef CS120_UNIX_SOCKET_HPP
#define CS120_UNIX_SOCKET_HPP


#include "utility.hpp"
#include "queue.hpp"
#include "mod.hpp"


namespace cs120 {
class UnixSocket : public BaseSocket {
private:

public:
    SPSCQueueSenderSlotGuard send() final;

    SPSCQueueReceiverSlotGuard recv() final;

    ~UnixSocket() override = default;
};
}


#endif //CS120_UNIX_SOCKET_HPP
