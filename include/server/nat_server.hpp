#ifndef CS120_NAT_SERVER_HPP
#define CS120_NAT_SERVER_HPP


#include <memory>
#include <mutex>

#include "device/mod.hpp"


namespace cs120 {
class NatServer {
private:
    static const uint16_t NAT_PORTS_BASE = 45678;

    static void *nat_lan_to_wan(void *args_);

    static void *nat_wan_to_lan(void *args_);

    pthread_t lan_to_wan, wan_to_lan;
    std::unique_ptr<BaseSocket> lan, wan;
    std::mutex lock;
    Array<std::pair<uint32_t, uint16_t>> nat_table;

public:
    NatServer(std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan);

    NatServer(NatServer &&other) = delete;

    NatServer &operator=(NatServer &&other) = delete;

    ~NatServer() = default;
};
}


#endif //CS120_NAT_SERVER_HPP
