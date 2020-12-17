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
    Demultiplexer::RequestSender recv_queue;
    MPSCQueue<PacketBuffer>::Sender send_queue;
    int athernet;

public:
    explicit UnixSocket(size_t size);

    UnixSocket(UnixSocket &&other) noexcept = default;

    UnixSocket &operator=(UnixSocket &&other) noexcept = default;

    size_t get_mtu() final { return ATHERNET_MTU - 1; }

    std::pair<MPSCQueue<PacketBuffer>::Sender, Demultiplexer::ReceiverGuard>
    bind(Demultiplexer::Condition &&condition, size_t size) final {
        return std::make_pair(send_queue, recv_queue.send(std::move(condition), size));
    }

    ~UnixSocket() override {
        pthread_join(receiver, nullptr);
        pthread_join(sender, nullptr);
        if (athernet != -1) { close(athernet); }
    }
};
}


#endif //CS120_UNIX_SOCKET_HPP
