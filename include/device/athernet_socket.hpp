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
    Demultiplexer<PacketBuffer>::RequestSender recv_queue;
    MPSCQueue<PacketBuffer>::Sender send_queue;
    int athernet;

public:
    explicit AthernetSocket(size_t size);

    AthernetSocket(AthernetSocket &&other) noexcept = default;

    AthernetSocket &operator=(AthernetSocket &&other) noexcept = default;

    uint16_t get_mtu() final { return ATHERNET_MTU - 1; }

    std::pair<MPSCQueue<PacketBuffer>::Sender, Demultiplexer<PacketBuffer>::ReceiverGuard>
    bind(Demultiplexer<PacketBuffer>::Condition &&condition, size_t size) final {
        return std::make_pair(send_queue, recv_queue.send(std::move(condition), size));
    }

    ~AthernetSocket() override {
        pthread_join(receiver, nullptr);
        pthread_join(sender, nullptr);
        if (athernet != -1) { close(athernet); }
    }
};
}


#endif //CS120_ATHERNET_SOCKET_HPP
