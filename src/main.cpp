#include "pcap/pcap.h"

#include "wire.hpp"
#include "utility.hpp"

using namespace cs120;

constexpr size_t BUFFER_SIZE = 4096;


void pcap_callback(u_char *arg, const struct pcap_pkthdr *info, const u_char *packet) {
    (void) arg;

    if (info->caplen != info->len) { cs120_warn("packet truncated!"); }

    Slice<uint8_t> eth_datagram{static_cast<const uint8_t *>(packet), info->caplen};

    auto *eth = (struct ethhdr *) eth_datagram.begin();

    if (eth->h_proto != 8) { return; }

    auto eth_data = eth_datagram[Range{sizeof(struct ethhdr), eth_datagram.size()}];

    auto *ip = (struct iphdr *) eth_data.begin();

    if (ip->iphdr_version != 4u) { return; }

    format(*ip);

    if (ip->iphdr_protocol != 17) { return; }

    auto ip_data = eth_data[Range{static_cast<size_t>(ip->iphdr_ihl) * 4, eth_data.size()}];
    auto *udp = (struct udphdr *) ip_data.begin();

    format(*udp);

    auto udp_data = ip_data[Range{sizeof(struct udphdr), ip_data.size()}];

    format(udp_data);
}


int main() {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *handle = pcap_open_live(device->name, BUFFER_SIZE, 0, 1, pcap_error);
    if (handle == nullptr) { cs120_abort(pcap_error); }

    pcap_freealldevs(device);

    struct bpf_program filter{};

    if (pcap_compile(handle, &filter, "", 0, PCAP_NETMASK_UNKNOWN) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(handle));
    }

    if (pcap_loop(handle, 10, pcap_callback, nullptr) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(handle));
    }

    pcap_close(handle);
}
