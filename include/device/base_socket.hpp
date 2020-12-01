#ifndef CS120_BASE_SOCKET_HPP
#define CS120_BASE_SOCKET_HPP


#include "queue.hpp"


namespace cs120 {
class BaseSocket {
public:
    virtual size_t get_mtu() = 0;

    virtual SPSCQueueSenderSlotGuard try_send() = 0;

    virtual SPSCQueueSenderSlotGuard send() = 0;

    virtual SPSCQueueReceiverSlotGuard try_recv() = 0;

    virtual SPSCQueueReceiverSlotGuard recv() = 0;

    virtual ~BaseSocket() = default;
};
}


#endif //CS120_BASE_SOCKET_HPP
