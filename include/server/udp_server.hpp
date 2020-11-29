#ifndef CS120_UDP_SERVER_HPP
#define CS120_UDP_SERVER_HPP


#include "device/mod.hpp"


namespace cs120 {
class UDPServer {
private:
    std::unique_ptr<BaseSocket> device;

    uint32_t src_ip, dest_ip;
    uint16_t src_port, dest_port;

public:
    UDPServer(std::unique_ptr<BaseSocket> device, uint32_t src_ip, uint32_t dest_ip,
              uint16_t src_port, uint16_t dest_port) :
            device{std::move(device)}, src_ip{src_ip}, dest_ip{dest_ip},
            src_port{src_port}, dest_port{dest_port} {}

    UDPServer(UDPServer &&other) noexcept = default;

    UDPServer &operator=(UDPServer &&other) noexcept = default;

    size_t send(Slice<uint8_t> data);

    size_t recv(MutSlice<uint8_t> data);

    ~UDPServer() = default;
};
}


#endif //CS120_UDP_SERVER_HPP
