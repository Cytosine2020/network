#include "device/raw_socket.hpp"

#include <numeric>

#include "pthread.h"


namespace {
using namespace cs120;

struct pcap_callback_args {
    pcap_t *pcap_handle;
    struct bpf_program filter;
};

void pcap_callback(u_char *arg, const struct pcap_pkthdr *info, const u_char *packet) {
    (void) arg;

    if (info->caplen != info->len) { cs120_warn("packet truncated!"); }

    Slice<uint8_t> eth_datagram{static_cast<const uint8_t *>(packet), info->caplen};

    auto *eth = (struct ethhdr *) eth_datagram.begin();

    if (eth->h_proto != 8) { return; }

    auto eth_data = eth_datagram[Range{sizeof(struct ethhdr)}];

    auto *ip = (struct iphdr *) eth_data.begin();

    if (ip->iphdr_version != 4u) { return; }

    format(*ip);

    if (ip->iphdr_protocol != 17) { return; }

    auto ip_data = eth_data[Range{static_cast<size_t>(ip->iphdr_ihl) * 4}];
    auto *udp = (struct udphdr *) ip_data.begin();

    format(*udp);

    auto udp_data = ip_data[Range{sizeof(struct udphdr)}];

    format(udp_data);
}

void *raw_socket_receiver(void *args_) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    auto count = std::numeric_limits<int32_t>::max();

    for (;;) {
        if (pcap_loop(args->pcap_handle, count, pcap_callback, nullptr) == PCAP_ERROR) {
            cs120_abort(pcap_geterr(args->pcap_handle));
        }
    }
}
}


namespace cs120 {
RawSocket::RawSocket() {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *pcap_handle = pcap_open_live(device->name, PCAP_BUFFER_SIZE, 0, 1, pcap_error);
    if (pcap_handle == nullptr) { cs120_abort(pcap_error); }

    pcap_freealldevs(device);

    auto *args = new pcap_callback_args{
        .pcap_handle = pcap_handle,
        .filter = (struct bpf_program){},
    };

    if (pcap_compile(pcap_handle, &args->filter, "", 0, PCAP_NETMASK_UNKNOWN) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(pcap_handle));
    }

    pthread_create(&receiver, nullptr, raw_socket_receiver, args);
}
}
