//
// Created by zhao on 2020/11/28.
//


#define MY_IP "192.168.1.2"
#define MAX_ICMP_FRAME 500

#include <string.h>
#include <zconf.h>
#include <libnet.h>
#include "icmp.hpp"
#include "utility.hpp"
namespace cs120 {
    size_t generate_IP(uint32_t addr, uint16_t i,MutSlice<uint8_t>frame,Slice<uint8_t> data) {

        auto *ip_header=frame.buffer_cast<struct ip>();

        ip_header->ip_hl = 5;
        ip_header->ip_v = 4;
        ip_header->ip_tos = 0;
        ip_header->ip_len = frame1.length + 20;
        ip_header->ip_id = i;
        ip_header->ip_off = IP_DF;
        ip_header->ip_ttl = 64;
        ip_header->ip_p = IPPROTO_ICMP;
        ip_header->ip_sum = 0;
        ip_header->ip_src.s_addr = inet_addr("192.168.1.1");
        ip_header->ip_dst.s_addr = addr;
        ip_header->ip_sum = checksum_fun((uint8_t *) ip_header, 20, 2);





    }

    size_t generate_icmp(Icmp_request opt, MutSlice<uint8_t> frame, uint16_t seq,Slice<uint8_t> data) {
        switch (opt) {
            case REQUEST: {
                auto *icmp_header = frame.buffer_cast<struct icmp>();

                icmp_header->code = 0;
                icmp_header->seq = seq;
                icmp_header->type = 8;
                icmp_header->sum = 0;

                (frame)[Range{sizeof(struct icmp)}][Range{0, data.size()}].copy_from_slice(data);


                return sizeof(struct icmp)+data.size();
            }
            case REPLY: {
                frame[Range{0,data.size()}].shallow_copy_from_slice(data);
                return data.size();
            }
        }
    }

    int checksum_fun(MutSlice<uint8_t> data, size_t length, int choice) {
        MutSlice<uint16_t>{reinterpret_cast<uint16_t *>(data.begin()), data.size() / 2};

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
        } else if (choice == 1) {
            if (sum != 0xFFFF) {
                return -1;
            }
        } else {
            return sum;
        }
        return 0;


    }
}