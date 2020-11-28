#ifndef CS120_UDP_SERVER_HPP
#define CS120_UDP_SERVER_HPP


#include "device/mod.hpp"


namespace cs120 {
class UDPServer {
private:
    std::unique_ptr<BaseSocket> device;

public:
    UDPServer(std::unique_ptr<BaseSocket> device) : device{std::move(device)} {}

    UDPServer(UDPServer &&other) noexcept = default;

    UDPServer &operator=(UDPServer &&other) noexcept = default;

    size_t send(Slice<uint8_t> data);

    size_t recv(MutSlice<uint8_t> data);

    ~UDPServer() = default;
};
}


#endif //CS120_UDP_SERVER_HPP
