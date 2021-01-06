#ifndef CS120_UDP_SERVER_HPP
#define CS120_UDP_SERVER_HPP


#include "device/base_socket.hpp"


namespace cs120 {
class UDPServer {
private:
    std::shared_ptr<BaseSocket> device;
    MPSCQueue<PacketBuffer>::Sender send_queue;
    Demultiplexer<PacketBuffer>::ReceiverGuard recv_queue;
    uint32_t src_ip, dest_ip;
    uint16_t src_port, dest_port;
    Array<uint8_t> receive_buffer;
    MutSlice<uint8_t> receive_buffer_slice;
    uint16_t identifier;

public:
    UDPServer(std::shared_ptr<BaseSocket> &device,  size_t size,
              uint32_t src_ip, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port);

    UDPServer(UDPServer &&other) noexcept = default;

    UDPServer &operator=(UDPServer &&other) noexcept = default;

    size_t send(Slice<uint8_t> data);

    size_t recv(MutSlice<uint8_t> data);

    ~UDPServer() = default;
};
}


#endif //CS120_UDP_SERVER_HPP
