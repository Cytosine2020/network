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

void pcap_callback(u_char *args_, const struct pcap_pkthdr *info, const u_char *packet) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    if (info->caplen != info->len) { cs120_abort("packet truncated!"); }

    Slice<uint8_t> eth_datagram{packet, info->caplen};

    auto *eth_header = eth_datagram.buffer_cast<struct ethhdr>();
    if (eth_header == nullptr || eth_header->h_proto != 8) { return; }

    auto ip_datagram = eth_datagram[Range{sizeof(struct ethhdr)}];

    size_t size = get_ipv4_data_size(ip_datagram);
    if (size == 0) { return; }

    auto slot = args->queue->try_send();

    if (slot->empty()) {
        cs120_warn("package loss!");
    } else {
        (*slot)[Range{0, size}].copy_from_slice(ip_datagram[Range{0, size}]);
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

void *raw_socket_sender(void *args_) {
    auto *queue = (SPSCQueue *) args_;

    for (;;) {
        char err_buf[LIBNET_ERRBUF_SIZE];
        libnet_t * context = libnet_init(LIBNET_RAW4, nullptr, err_buf);
        if (context == nullptr) { cs120_abort(err_buf); }

        auto buffer = queue->recv();
        auto *ip_header = buffer->buffer_cast<struct ip>();

        auto icmp_datagram = (*buffer)[Range{sizeof(struct ip)}];

        auto *icmp_header = icmp_datagram.buffer_cast<struct icmp>();
        auto payload = icmp_datagram[Range{sizeof(struct icmp)}];

        if (libnet_build_icmpv4_echo(icmp_header->type, 0, 0, icmp_header->ident,
                                     icmp_header->seq, payload.begin(),
                                     sizeof(struct timeval), context, 0) == -1) {
            cs120_abort("Can't create IP header");
        }

        if (libnet_build_ipv4(ip_header->ip_len, ip_header->ip_tos, ip_header->ip_id,
                              ip_header->ip_off, ip_header->ip_ttl, ip_header->ip_p,
                              ip_header->ip_sum, ip_header->ip_src.s_addr,
                              ip_header->ip_dst.s_addr, nullptr, 0, context, 0) == -1) {
            cs120_abort("Can't create IP header");
        }

        if (libnet_write(context) == -1) { cs120_abort(libnet_geterror(context)); }

        libnet_destroy(context);
    }
}
}


namespace cs120 {
RawSocket::RawSocket(size_t buffer_size, size_t size) :
        receiver{}, sender{}, receive_queue{nullptr}, send_queue{nullptr} {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *pcap_handle = pcap_open_live(device->name, buffer_size, 0, 1, pcap_error);
    if (pcap_handle == nullptr) { cs120_abort(pcap_error); }

    pcap_freealldevs(device);

    receive_queue = new SPSCQueue{buffer_size, size};
    send_queue = new SPSCQueue{buffer_size, size};

    auto *args = new pcap_callback_args{
            .pcap_handle = pcap_handle,
            .filter = (struct bpf_program) {},
            .queue = receive_queue,
    };

    char expr[] = "(icmp or udp)";

    if (pcap_compile(pcap_handle, &args->filter, expr, 0, PCAP_NETMASK_UNKNOWN) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(pcap_handle));
    }

    pthread_create(&receiver, nullptr, raw_socket_receiver, args);
    pthread_create(&sender, nullptr, raw_socket_sender, send_queue);
}
}
