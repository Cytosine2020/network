#include <vector>
#include <numeric>
#include "pthread.h"

#include "device/raw_socket.hpp"


namespace {
using namespace cs120;

struct pcap_callback_args {
    pcap_t *pcap_handle;
    struct bpf_program filter;
    SPSCQueue *queue;
};

// todo: check if datagram is valid
void pcap_callback(u_char *args_, const struct pcap_pkthdr *info, const u_char *packet) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    if (info->caplen != info->len) { cs120_warn("packet truncated!"); }

    Slice<uint8_t> eth_datagram{packet, info->caplen};

    auto *eth_header = eth_datagram.buffer_cast<struct ethhdr>();
    if (eth_header != nullptr && eth_header->h_proto != 8) { return; }

    auto ip_datagram = eth_datagram[Range{sizeof(struct ethhdr)}];

    auto *ip_header = ip_datagram.buffer_cast<struct iphdr>();
    if (ip_header != nullptr && ip_datagram.size() < iphdr_tot_len(*ip_header)) { return; }

    if (!args->queue->send(ip_datagram[Range{0, iphdr_tot_len(*ip_header)}])) {
        cs120_warn("package loss!");
    }
}

void *raw_socket_receiver(void *args_) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);
    auto *pcap_args = reinterpret_cast<u_char *>(args);
    auto count = std::numeric_limits<int32_t>::max();

    for (;;) {
        if (pcap_loop(args->pcap_handle, count, pcap_callback, pcap_args) == PCAP_ERROR) {
            cs120_abort(pcap_geterr(args->pcap_handle));
        }
    }
}
}


namespace cs120 {
RawSocket::RawSocket() : receiver{}, receive_queue{nullptr} {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *pcap_handle = pcap_open_live(device->name, SOCKET_BUFFER_SIZE, 0, 1, pcap_error);
    if (pcap_handle == nullptr) { cs120_abort(pcap_error); }

    pcap_freealldevs(device);

    receive_queue = new SPSCQueue{SOCKET_BUFFER_SIZE, 16};

    auto *args = new pcap_callback_args{
            .pcap_handle = pcap_handle,
            .filter = (struct bpf_program) {},
            .queue = receive_queue,
    };

    if (pcap_compile(pcap_handle, &args->filter, "", 0, PCAP_NETMASK_UNKNOWN) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(pcap_handle));
    }

    pthread_create(&receiver, nullptr, raw_socket_receiver, args);
}

Array<uint8_t> RawSocket::recv() {
    return receive_queue->recv();
}
}
