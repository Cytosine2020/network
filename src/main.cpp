#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <arpa/inet.h>
#include <netinet/udp.h>


int main() {
    int sock_r = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_r < 0) {
        printf("error in socket\n");
        return -1;
    }

    auto *buffer = new uint8_t[65536]{};

    struct sockaddr saddr{};

    int saddr_len = sizeof(saddr);

    ssize_t buflen = recvfrom(sock_r, buffer, 65536, 0, &saddr, (socklen_t *) &saddr_len);

    if (buflen < 0) {
        printf("error in reading recvfrom function\n");
        return -1;
    }

    auto *eth = (struct ethhdr *) (buffer);
    printf("\nEthernet Header\n");
    printf("\t|-Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", eth->h_source[0],
           eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4],
           eth->h_source[5]);
    printf("\t|-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n", eth->h_dest[0],
           eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
    printf("\t|-Protocol : %d\n", eth->h_proto);

    unsigned short iphdrlen;
    auto *ip = (struct iphdr *) (buffer + sizeof(struct ethhdr));
    iphdrlen = ip->ihl*4;

    struct sockaddr_in source{};
    source.sin_addr.s_addr = ip->saddr;

    struct sockaddr_in dest{};
    dest.sin_addr.s_addr = ip->daddr;

    printf("\t|-Version : %d\n", (unsigned int) ip->version);
    printf("\t|-Internet Header Length : %d DWORDS or %d Bytes\n", (unsigned int) ip->ihl,
           ((unsigned int) (ip->ihl)) * 4);
    printf("\t|-Type Of Service : %d\n", (unsigned int) ip->tos);
    printf("\t|-Total Length : %d Bytes\n", ntohs(ip->tot_len));
    printf("\t|-Identification : %d\n", ntohs(ip->id));
    printf("\t|-Time To Live : %d\n", (unsigned int) ip->ttl);
    printf("\t|-Protocol : %d\n", (unsigned int) ip->protocol);
    printf("\t|-Header Checksum : %d\n", ntohs(ip->check));
    printf("\t|-Source IP : %s\n", inet_ntoa(source.sin_addr));
    printf("\t|-Destination IP : %s\n", inet_ntoa(dest.sin_addr));

    auto *udp = (struct udphdr *) (buffer + iphdrlen + sizeof(struct ethhdr));

    printf("\t|-Source Port : %d\n", ntohs(udp->source));
    printf("\t|-Destination Port : %d\n", ntohs(udp->dest));
    printf("\t|-UDP Length : %d\n", ntohs(udp->len));
    printf("\t|-UDP Checksum : %d\n", ntohs(udp->check));

    unsigned char *data = (buffer + iphdrlen + sizeof(struct ethhdr) + sizeof(struct udphdr));

    size_t remaining_data = buflen - (iphdrlen + sizeof(struct ethhdr) + sizeof(struct udphdr));

    for (size_t i = 0; i < remaining_data; i++) {
        if (i != 0 && i % 16 == 0) printf("\n");
        printf("%.2X ", data[i]);
    }
}
