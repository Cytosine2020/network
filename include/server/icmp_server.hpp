#ifndef CS120_ICMP_SERVER_HPP
#define CS120_ICMP_SERVER_HPP


#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "device/mod.hpp"


namespace cs120 {
void icmp_ping(std::unique_ptr<BaseSocket> sock, uint32_t dest);
}


#endif //CS120_ICMP_SERVER_HPP
