#ifndef CS120_NAT_SERVER_HPP
#define CS120_NAT_SERVER_HPP


#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

#include "device/base_socket.hpp"
#include "wire/wire.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"
#include "wire/udp.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
class NatServer {
private:
    static void *nat_lan_to_wan(void *args) {
        reinterpret_cast<NatServer *>(args)->nat_lan_to_wan();
        return nullptr;
    }

    static void *nat_wan_to_lan(void *args) {
        reinterpret_cast<NatServer *>(args)->nat_wan_to_lan();
        return nullptr;
    }

    void nat_lan_to_wan();

    void nat_wan_to_lan();

    pthread_t lan_to_wan, wan_to_lan;
    std::shared_ptr<BaseSocket> lan, wan;

    MPSCQueue<PacketBuffer>::Sender lan_sender, wan_sender;
    Demultiplexer::ReceiverGuard lan_receiver, wan_receiver;

    Array<std::atomic<EndPoint>> nat_table; // shared between lan to wan and wan to lan
    std::unordered_map<EndPoint, uint16_t> nat_reverse_table;
    size_t lowest_free_port;

    uint32_t wan_addr;

public:
    static const uint16_t NAT_PORTS_BASE = 50000;
    static const uint16_t NAT_PORTS_SIZE = 1024;

    NatServer(uint32_t lan_addr, uint32_t wan_addr,
              std::shared_ptr<BaseSocket> lan, std::shared_ptr<BaseSocket> wan,
              size_t size, const Array<EndPoint> &map_addr);

    NatServer(NatServer &&other) = delete;

    NatServer &operator=(NatServer &&other) = delete;

    ~NatServer() {
        pthread_join(lan_to_wan, nullptr);
        pthread_join(wan_to_lan, nullptr);
    };
};
}


#endif //CS120_NAT_SERVER_HPP
