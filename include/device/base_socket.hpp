#ifndef CS120_BASE_SOCKET_HPP
#define CS120_BASE_SOCKET_HPP


#include "utility.hpp"
#include "queue.hpp"


namespace cs120 {
using PacketBuffer = Buffer<uint8_t, 2048>;


class BaseSocket {
public:
    virtual size_t get_mtu() = 0;

    virtual MPSCSenderSlotGuard<PacketBuffer> try_send() = 0;

    virtual MPSCSenderSlotGuard<PacketBuffer> send() = 0;

    virtual MPSCReceiverSlotGuard<PacketBuffer> try_recv() = 0;

    virtual MPSCReceiverSlotGuard<PacketBuffer> recv() = 0;

    virtual ~BaseSocket() = default;
};
}


#endif //CS120_BASE_SOCKET_HPP
