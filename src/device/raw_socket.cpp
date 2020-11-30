#include <numeric>
#include "pthread.h"

#include "device/raw_socket.hpp"


namespace {
using namespace cs120;

struct pcap_callback_args {
    pcap_t *pcap_handle;
    struct bpf_program filter;
    SPSCQueue *queue;
    uint32_t ip_addr;
};

void pcap_callback(u_char *args_, const struct pcap_pkthdr *info, const u_char *packet) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    if (info->caplen != info->len) {
        cs120_warn("packet truncated!");
        return;
    }

    Slice<uint8_t> eth_datagram{packet, info->caplen};

    auto *eth_header = eth_datagram.buffer_cast<struct ethhdr>();
    if (eth_header == nullptr || eth_header->h_proto != 8) { return; }

    auto *ip_header = eth_datagram.buffer_cast<struct ip>();

    uint32_t src_ip = ip_get_saddr(*ip_header).s_addr;
    if (src_ip == args->ip_addr) { return; }

    auto ip_datagram = eth_datagram[Range{sizeof(struct ethhdr)}];

    size_t size = get_ipv4_total_size(ip_datagram);
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

struct raw_socket_sender_args {
    libnet_t *context;
    SPSCQueue *queue;
};

void *raw_socket_sender(void *args_) {
    auto *args = reinterpret_cast<raw_socket_sender_args *>(args_);

    for (;;) {
        auto buffer = args->queue->recv();

        size_t size = get_ipv4_total_size(*buffer);
        if (size == 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto *ip_header = buffer->buffer_cast<struct ip>();
        auto ip_data = (*buffer)[Range{ip_get_ihl(*ip_header), size}];

        if (libnet_build_ipv4(ip_header->ip_len, ip_header->ip_tos, ip_header->ip_id,
                              ip_header->ip_off, ip_header->ip_ttl, ip_header->ip_p,
                              ip_header->ip_sum, ip_header->ip_src.s_addr,
                              ip_header->ip_dst.s_addr, ip_data.begin(), ip_data.size(),
                              args->context, 0) == -1) {
            cs120_abort(libnet_geterror(args->context));
        }

        if (libnet_write(args->context) == -1) { cs120_abort(libnet_geterror(args->context)); }

        libnet_clear_packet(args->context);
    }
}
}


namespace cs120 {
RawSocket::RawSocket(size_t size, uint32_t ip_addr) :
        receiver{}, sender{}, receive_queue{nullptr}, send_queue{nullptr} {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *pcap_handle = pcap_open_live(device->name, static_cast<int32_t>(get_mtu() + 100u),
                                         0, 1, pcap_error);
    if (pcap_handle == nullptr) { cs120_abort(pcap_error); }

    char err_buf[LIBNET_ERRBUF_SIZE];
    libnet_t *context = libnet_init(LIBNET_RAW4_ADV, device->name, err_buf);
    if (context == nullptr) { cs120_abort(err_buf); }

    pcap_freealldevs(device);

    receive_queue = new SPSCQueue{get_mtu(), size};
    send_queue = new SPSCQueue{get_mtu(), size};

    auto *recv_args = new pcap_callback_args{
            .pcap_handle = pcap_handle,
            .filter = (struct bpf_program) {},
            .queue = receive_queue,
            .ip_addr = ip_addr,
    };

    auto *send_args = new raw_socket_sender_args{
            .context = context,
            .queue = send_queue,
    };

    char expr[100]{};
    snprintf(expr, 100, "(icmp or udp) and (dst host %s)", inet_ntoa(in_addr{ip_addr}));

    if (pcap_compile(pcap_handle, &recv_args->filter, "", 0, PCAP_NETMASK_UNKNOWN) ==
        PCAP_ERROR) {
        cs120_abort(pcap_geterr(pcap_handle));
    }

    pthread_create(&receiver, nullptr, raw_socket_receiver, recv_args);
    pthread_create(&sender, nullptr, raw_socket_sender, send_args);
}
}
