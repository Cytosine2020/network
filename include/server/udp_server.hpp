#ifndef CS120_UDP_SERVER_HPP
#define CS120_UDP_SERVER_HPP


#include "device/mod.hpp"


namespace cs120 {
class UDPServer {
private:
    std::unique_ptr<BaseSocket> device;

    uint32_t src_ip, dest_ip;
    uint16_t src_port, dest_port;
    Array<uint8_t> receive_buffer;
    MutSlice<uint8_t> receive_buffer_slice;
    uint16_t identifier;

public:
    UDPServer(std::unique_ptr<BaseSocket> device, uint32_t src_ip, uint32_t dest_ip,
              uint16_t src_port, uint16_t dest_port) :
            device{std::move(device)}, src_ip{src_ip}, dest_ip{dest_ip},
            src_port{src_port}, dest_port{dest_port},
            receive_buffer{this->device->get_mtu()},
            receive_buffer_slice{receive_buffer.begin(), 0}, identifier{0} {}

    UDPServer(UDPServer &&other) noexcept = default;

    UDPServer &operator=(UDPServer &&other) noexcept = default;

    size_t send(Slice<uint8_t> data);

    size_t recv(MutSlice<uint8_t> data);

    ~UDPServer() = default;
};
}


#endif //CS120_UDP_SERVER_HPP
