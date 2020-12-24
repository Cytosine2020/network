#include "device/raw_socket.hpp"

#include "pthread.h"
#include <numeric>

#include "pcap/pcap.h"
#include "libnet.h"

#include "wire/wire.hpp"
#include "wire/ipv4.hpp"


namespace {
using namespace cs120;


struct raw_socket_sender_args {
    libnet_t *context;
    MPSCQueue<PacketBuffer>::Receiver queue;
};

void *raw_socket_sender(void *args_) {
    auto *args = reinterpret_cast<raw_socket_sender_args *>(args_);

    for (;;) {
        auto buffer = args->queue.recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*buffer)[Range{}]);
        if (ip_header == nullptr) {
            cs120_warn("invalid package!");
            continue;
        }

        if (libnet_build_ipv4(ip_header->get_total_length(), ip_header->get_type_of_service(),
                              ip_header->get_identification(), ip_header->get_fragment(),
                              ip_header->get_time_to_live(),
                              static_cast<uint8_t>(ip_header->get_protocol()),
                              ip_header->get_checksum(), ip_header->get_src_ip(),
                              ip_header->get_dest_ip(), ip_data.begin(), ip_data.size(),
                              args->context, 0) == -1) {
            cs120_abort(libnet_geterror(args->context));
        }

        if (!ip_option.empty() && libnet_build_ipv4_options(
                ip_option.begin(), ip_option.size(), args->context, 0) == -1) {
            cs120_abort(libnet_geterror(args->context));
        }

        if (libnet_write(args->context) == -1) { cs120_warn(libnet_geterror(args->context)); }

        libnet_clear_packet(args->context);
    }
}


struct pcap_callback_args {
    pcap_t *pcap_handle;
    Demultiplexer demultiplexer;
};

void pcap_callback(u_char *args_, const struct pcap_pkthdr *info, const u_char *packet) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    if (info->caplen != info->len) {
        cs120_warn("packet truncated!");
        return;
    }

    Slice<uint8_t> eth_datagram{packet, info->caplen};

    auto *eth_header = eth_datagram.buffer_cast<ETHHeader>();
    if (eth_header == nullptr) {
        cs120_warn("invalid package!");
        return;
    }

    if (eth_header->protocol != 8) { return; }

    auto eth_data = eth_datagram[Range(sizeof(ETHHeader))];

    args->demultiplexer.send(eth_data);
}

void *raw_socket_receiver(void *args_) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);
    auto *pcap_args = reinterpret_cast<u_char *>(args_);
    auto count = std::numeric_limits<int32_t>::max();

    for (;;) {
        if (pcap_loop(args->pcap_handle, count, pcap_callback, pcap_args) == PCAP_ERROR) {
            cs120_abort(pcap_geterr(args->pcap_handle));
        }
    }
}
}


namespace cs120 {
RawSocket::RawSocket(size_t size) :
        receiver{}, sender{}, recv_queue{}, send_queue{} {
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

    auto[send_sender, send_receiver] = MPSCQueue<PacketBuffer>::channel(size);

    auto *recv_args = new pcap_callback_args{
            .pcap_handle = pcap_handle,
            .demultiplexer = Demultiplexer{size},
    };

    auto *send_args = new raw_socket_sender_args{
            .context = context,
            .queue = std::move(send_receiver),
    };

    recv_queue = recv_args->demultiplexer.get_sender();
    send_queue = std::move(send_sender);

    pthread_create(&receiver, nullptr, raw_socket_receiver, recv_args);
    pthread_create(&sender, nullptr, raw_socket_sender, send_args);
}
}
