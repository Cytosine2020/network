//
// Created by zhao on 2020/11/28.
//

#ifndef CS120_ICMP_H
#define CS120_ICMP_H
#include <stdint.h>
#include "utility.hpp"
#include <netinet/in.h>
#include "malloc.h"
#include "sys/socket.h"
#include "queue.hpp"
#include "device/raw_socket.hpp"
#include "netinet/ip.h"
namespace cs120 {
    struct data_pass {
        uint8_t *data;
        size_t length;
    };
    struct receive_args{
        RawSocket * raw;
        int pid;
    };

    enum Icmp_request {
        REQUEST,
        REPLY
    };

    struct socket_ret {
        struct sockaddr_in sockin;
        int socket_id;
        char *data;
    };

    int send_data(RawSocket *raw);
    void *receive_data(void* args_);
    size_t generate_icmp(Icmp_request opt, MutSlice<uint8_t> frame, uint16_t seq, Slice<uint8_t> data);
    size_t generate_IP(uint32_t addr, uint16_t i,MutSlice<uint8_t> frame,Slice<uint8_t> data,Icmp_request opt);



}
#endif //CS120_ICMP_H
