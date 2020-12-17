#ifndef CS120_ICMP_SERVER_HPP
#define CS120_ICMP_SERVER_HPP


#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
class ICMPPing {
private:
    MPSCQueue<PacketBuffer>::Sender send_queue;
    Demultiplexer::ReceiverGuard recv_queue;
    uint32_t src_ip, dest_ip;
    uint16_t src_port, dest_port;
    uint16_t identification;

public:
    ICMPPing(MPSCQueue<PacketBuffer>::Sender send_queue, Demultiplexer::ReceiverGuard recv_queue,
             uint32_t src_ip, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port,
             uint16_t identification) :
            send_queue{std::move(send_queue)}, recv_queue{std::move(recv_queue)},
            src_ip{src_ip}, dest_ip{dest_ip}, src_port{src_port}, dest_port{dest_port},
            identification{identification} {}

    bool ping(uint16_t seq);
};

class ICMPServer {
private:
    static constexpr size_t BUFFER_SIZE = 64;

    std::shared_ptr<BaseSocket> device;
    pthread_t receiver;
    MPSCQueue<PacketBuffer>::Sender send_queue;
    Demultiplexer::ReceiverGuard recv_queue;
    uint32_t ip_addr;

    static void *icmp_receiver(void *args) {
        reinterpret_cast<ICMPServer *>(args)->icmp_receiver();
        return nullptr;
    }

    void icmp_receiver();

public:
    explicit ICMPServer(std::shared_ptr<BaseSocket> device, uint32_t ip_addr);

    ICMPServer(ICMPServer &&other) noexcept = delete;

    ICMPServer &operator=(ICMPServer &&other) noexcept = delete;

    ICMPPing create_ping(uint16_t identification, uint32_t dest_ip,
                         uint16_t src_port, uint16_t dest_port);

    ~ICMPServer() = default;
};
}


#endif //CS120_ICMP_SERVER_HPP
