//
// Created by zhao on 2020/11/29.
//

#ifndef CS120_UDP_DATA_H
#define CS120_UDP_DATA_H

#include "iostream"
#include "device/unix_socket.hpp"
struct udp{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum=0;
};
#endif //CS120_UDP_DATA_H
