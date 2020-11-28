//
// Created by zhao on 2020/11/28.
//

#ifndef CS120_ICMP_H
#define CS120_ICMP_H
#include <stdint.h>
#include <netinet/in.h>
#include "malloc.h"
#include "sys/socket.h"
#include "netinet/ip.h"

struct icmp_frame {
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

int checksum_fun(uint8_t *data, size_t length, int choice);

struct icmp_frame
generate_icmp(enum Icmp_request type, uint16_t identifier, uint16_t sequence, uint8_t *data, size_t length);

#endif //CS120_ICMP_H
