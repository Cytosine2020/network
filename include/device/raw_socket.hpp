#ifndef CS120_RAW_SOCKET_HPP
#define CS120_RAW_SOCKET_HPP

#include <unistd.h>
#include "pthread.h"

#include "utility.hpp"
#include "queue.hpp"
#include "base_socket.hpp"


namespace cs120 {
class RawSocket : public BaseSocket {
private:
    pthread_t receiver, sender;
    Demultiplexer::RequestSender recv_queue;
    MPSCQueue<PacketBuffer>::Sender send_queue;

public:
    explicit RawSocket(size_t size);

    RawSocket(RawSocket &&other) noexcept = default;

    RawSocket &operator=(RawSocket &&other) noexcept = default;

    size_t get_mtu() final { return 1500; }

    std::pair<MPSCQueue<PacketBuffer>::Sender, Demultiplexer::ReceiverGuard>
    bind(Demultiplexer::Condition &&condition, size_t size) final {
        return std::make_pair(send_queue, recv_queue.send(std::move(condition), size));
    }

    ~RawSocket() override = default;
};
}


#endif //CS120_RAW_SOCKET_HPP
