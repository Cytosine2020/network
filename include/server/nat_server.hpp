#ifndef CS120_NAT_SERVER_HPP
#define CS120_NAT_SERVER_HPP


#include <memory>

#include "device/mod.hpp"


namespace cs120 {
class NatServer {
private:
    std::unique_ptr<BaseSocket> lan, wan;

public:
    NatServer(std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan) :
            lan{std::move(lan)}, wan{std::move(wan)} {}

    NatServer(NatServer &&other) = default;

    NatServer &operator=(NatServer &&other) = default;

    void start();

    ~NatServer() = default;
};
}


#endif //CS120_NAT_SERVER_HPP
