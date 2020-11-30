//
// Created by zhao on 2020/11/29.
//
#include <fstream>
#include "udp_data.hpp"

namespace cs120 {
    void send_udpfile(char * file_path, std::unique_ptr<BaseSocket> wan, uint16_t src_port, uint16_t dst_port, uint16_t dst_ip,
                      uint16_t src_ip) {
        std::ifstream in(file_path,std::ios::binary);
        in.seekg(0, in.end);
        size_t length = in.tellg();
        in.seekg(0, in.beg);

        size_t mtu_len = wan->get_mtu() - sizeof(struct ip) - sizeof(struct udp);
        uint8_t *buffer1 = new uint8_t[mtu_len];
        if (mtu_len < length) {
            while (length > mtu_len) {
                length -= mtu_len;
                in.read(reinterpret_cast<char *>(buffer1), mtu_len);
                auto buffer = wan->send();
                size_t udp_size = sizeof(struct udp) + mtu_len;
                generate_ip(*buffer, IPPROTO_UDP, src_ip, dst_ip, udp_size);
                auto udp_frame = (*buffer)[Range{sizeof(struct ip)}];
                auto *udp_header = udp_frame.buffer_cast<struct udp>();
                udp_header->length = htons(udp_size);
                udp_header->src_port = htons(src_port);
                udp_header->dst_port = htons(dst_port);
                udp_frame[Range{sizeof(struct udp)}][Range{0, mtu_len}].copy_from_slice(
                        Slice<uint8_t>(buffer1, mtu_len));
            }
        }
        in.read(reinterpret_cast<char *>(buffer1), length);
        auto buffer = wan->send();
        size_t udp_size = sizeof(struct udp) + length;
        generate_ip(*buffer, IPPROTO_UDP, src_ip, dst_ip, udp_size);
        auto udp_frame = (*buffer)[Range{sizeof(struct ip)}];
        auto *udp_header = udp_frame.buffer_cast<struct udp>();
        udp_header->length = htons(udp_size);
        udp_header->src_port = htons(src_port);
        udp_header->dst_port = htons(dst_port);
        in.read(reinterpret_cast<char *>(buffer1), length);
        udp_frame[Range{sizeof(struct udp)}][Range{0, length}].copy_from_slice(
                Slice<uint8_t>(buffer1, length));
    }


    int main(int argc, char *args[]) {
        //4 arguments: file_path, source port, destination port, destination IP
        if (argc < 5) {
            printf("Need more args");
            return -1;
        }
        char *file_path = args[1];
        uint16_t src_port=strtoul(args[2], nullptr,10);
        uint16_t dst_port=strtoul(args[3], nullptr,10);
        uint32_t dst_address=inet_addr(args[4]);

        uint32_t src_address=inet_addr("192.168.1.1");

        std::unique_ptr<BaseSocket> wan(new UnixSocket{64});

        send_udpfile(file_path,move(wan),src_port,dst_port,dst_address,src_address);

    }
}