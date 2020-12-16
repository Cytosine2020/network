#ifndef CS120_ICMP_SERVER_HPP
#define CS120_ICMP_SERVER_HPP


#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
class ICMPServer {
private:
    static constexpr size_t BUFFER_SIZE = 64;
    using PingBuffer = Buffer<uint8_t, BUFFER_SIZE>;

    std::unique_ptr<BaseSocket> device;
    pthread_t receiver;
    MPSCSender<PingBuffer> send_queue;
    MPSCReceiver<PingBuffer> recv_queue;
    uint16_t identification;

    static void *icmp_receiver(void *args) {
        reinterpret_cast<ICMPServer *>(args)->icmp_receiver();
        return nullptr;
    }

    void icmp_receiver();

public:
    explicit ICMPServer(std::unique_ptr<BaseSocket> device, uint16_t identification);

    ICMPServer(ICMPServer &&other) noexcept = delete;

    ICMPServer &operator=(ICMPServer &&other) noexcept = delete;

    bool ping(uint16_t seq, uint32_t src_ip, uint32_t dest_ip,
              uint16_t src_port, uint16_t dest_port);

    ~ICMPServer() = default;
};
}


#endif //CS120_ICMP_SERVER_HPP
