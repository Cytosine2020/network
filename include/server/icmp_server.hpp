#ifndef CS120_ICMP_SERVER_HPP
#define CS120_ICMP_SERVER_HPP


#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t src_ip, uint32_t dest_ip,
               uint16_t src_port);
}


#endif //CS120_ICMP_SERVER_HPP
