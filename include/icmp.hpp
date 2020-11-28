//
// Created by zhao on 2020/11/28.
//

#ifndef CS120_ICMP_H
#define CS120_ICMP_H
#include <stdint.h>
#include "utility"
#include <netinet/in.h>
#include "malloc.h"
#include "sys/socket.h"
#include "netinet/ip.h"

struct data_pass {
    uint8_t *data;
    size_t length;
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
struct icmp{
    uint8_t type;
    uint8_t code;
    uint16_t sum;
    uint16_t ident;
    uint16_t seq;
};



#endif //CS120_ICMP_H
