//
// Created by zhao on 2020/11/28.
//


#define MY_IP "192.168.1.2"
#define MAX_ICMP_FRAME 500

#include <string.h>
#include <zconf.h>
#include <libnet.h>
#include "icmp.hpp"

#include "wire.hpp"



namespace cs120 {
    int send_data(RawSocket *raw){
        uint32_t addr=inet_addr("182.61.200.7");
        for (int i=0;i<10;i++){
            {
                auto buffer=raw->send();
                Array<uint8_t> times=Array<uint8_t>(sizeof(struct timeval));
                auto *start=times.buffer_cast<struct timeval>();
                gettimeofday(start, nullptr);
                generate_IP(addr,i,*buffer,times,REQUEST);
            }
            {
                auto buffer=raw->recv();
                auto *ip_header = buffer->buffer_cast<struct ip>();

                auto ip_datagram = (*buffer)[Range{0, ip_get_tot_len(*ip_header)}];

            }

            sleep(1);
        }
    }




    size_t generate_IP(uint32_t addr, uint16_t i,MutSlice<uint8_t> frame,Slice<uint8_t> data,Icmp_request opt) {

        auto *ip_header=frame.buffer_cast<struct ip>();

        ip_header->ip_hl = 5;
        ip_header->ip_v = 4;
        ip_header->ip_tos = 0;
        ip_header->ip_len = htons(data.size()+sizeof(struct ip)+sizeof(struct icmp));
        ip_header->ip_id = getpid();
        ip_header->ip_off = IP_DF;
        ip_header->ip_ttl = 64;
        ip_header->ip_p = IPPROTO_ICMP;
        ip_header->ip_sum = 0;
        ip_header->ip_src.s_addr = inet_addr("192.168.1.1");
        ip_header->ip_dst.s_addr = addr;
        ip_header->ip_sum = composite_checksum(frame[Range{0,sizeof(struct ip)}]);
        size_t length=generate_icmp(opt, frame[Range{sizeof(struct ip)}], i, data);
        return length+sizeof(struct ip);






    }

    size_t generate_icmp(Icmp_request opt, MutSlice<uint8_t> frame, uint16_t seq,Slice<uint8_t> data) {
        switch (opt) {
            case REQUEST: {
                auto *icmp_header = frame.buffer_cast<struct icmp>();

                icmp_header->code = 0;
                icmp_header->seq = seq;
                icmp_header->ident=getpid();
                icmp_header->type = 8;
                icmp_header->sum = 0;

                (frame)[Range{sizeof(struct icmp)}][Range{0, data.size()}].copy_from_slice(data);
                icmp_header->sum=composite_checksum(frame[Range{0,data.size()+sizeof(struct icmp)}]);
                return sizeof(struct icmp)+data.size();
            }
            case REPLY: {
                frame[Range{0,data.size()}].shallow_copy_from_slice(data);
                auto *icmp_header = frame.buffer_cast<struct icmp>();
                icmp_header->type=0;
                icmp_header->sum = 0;
                icmp_header->sum=composite_checksum(frame[Range{0,data.size()}]);

                return data.size();
            }
        }
    }


}