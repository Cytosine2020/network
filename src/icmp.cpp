//
// Created by zhao on 2020/11/28.
//


#define MY_IP "192.168.1.2"
#define MAX_ICMP_FRAME 500

#include <string.h>
#include <zconf.h>
#include <libnet.h>
#include "icmp.hpp"

char *generate_IP(int socket_flag, uint32_t addr, uint16_t i) {
    struct icmp_frame frame1,frame2;
    struct socket_ret info;
    int len = sizeof(struct timeval);
    uint16_t ident = getpid();

    struct timeval *start = (timeval *)malloc(sizeof(struct timeval));
    gettimeofday(start, NULL);

    frame1 = generate_icmp(REQUEST, ident, i, reinterpret_cast<uint8_t *>(&start), len);


    struct ip* ip_header=new ip();
    ip_header->ip_hl=5;
    ip_header->ip_v=4;
    ip_header->ip_tos=0;
    ip_header->ip_len=frame1.length+20;
    ip_header->ip_id=i;
    ip_header->ip_off=IP_DF;
    ip_header->ip_ttl=64;
    ip_header->ip_p=IPPROTO_ICMP;
    ip_header->ip_sum=0;
    ip_header->ip_src.s_addr=inet_addr("192.168.1.1");
    ip_header->ip_dst.s_addr=addr;
    ip_header->ip_sum=checksum_ip((uint8_t*)ip_header,20);




    char *tonode = malloc(frame1.length + 20);
    uint32_t *ptr = tonode;
    memcpy(ptr,ip_header,20);
    memcpy(ptr + 5, frame1.data, frame1.length);
    return tonode;


}

struct icmp_frame
generate_icmp(enum Icmp_request type, uint16_t identifier, uint16_t sequence, uint8_t *data, size_t length) {
    int icmp_len = length;
    uint8_t *frame =(uint8_t *) malloc(MAX_ICMP_FRAME);
    uint8_t *pointer = frame;
    *(pointer + 1) = 0;
    uint16_t *ptr2 = (uint16_t *) (pointer + 2);
    *ptr2 = 0;
    *(ptr2 + 1) = identifier;
    *(ptr2 + 2) = sequence;
    switch (type) {
        case REQUEST: {
            *pointer = 8;
            pointer += 8;
            for (int i = 0; i < length; i++) {
                pointer[i] = data[i];
            }
            pointer -= 8;
            icmp_len = icmp_len + 8;
            checksum_fun(pointer, icmp_len, 0);
            struct icmp_frame ret = {frame, static_cast<size_t>(icmp_len)};
            return ret;
        }
        case REPLY:
            *pointer = 0;
            memcpy(ptr2 + 3, data, length);
            icmp_len += icmp_len + 8;
            checksum_fun(pointer, icmp_len, 0);
            struct icmp_frame ret1 = {frame, static_cast<size_t>(icmp_len)};
            return ret1;
    }
}

int checksum_fun(uint8_t *data, size_t length, int choice) {
    uint16_t *ptr = (uint16_t *)data;
    if (length % 2) {
        data[length] = 0;
        length += 1;
    }
    uint32_t sum = 0;
    for (int i = 0; i < length / 2; i++) {
        sum += ptr[i];
        if (sum & 0x10000) {
            sum = (sum & 0xFFFF) + 1;
        }

    }
    sum = sum ^ 0xFFFF;
    if (choice == 0) {
        ptr[1] = sum;
    } else if (choice==1){
        if (sum != 0xFFFF) {
            return -1;
        }
    } else{
        return sum;
    }
    return 0;


}