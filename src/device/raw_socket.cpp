#include <queue>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include "pthread.h"

#include "device/raw_socket.hpp"


namespace {
using namespace cs120;

struct pcap_callback_args {
    pcap_t *pcap_handle;
    struct bpf_program filter;
    std::mutex *lock;
    std::condition_variable *variable;
    std::queue<std::vector<uint8_t>> *queue;
};

void pcap_callback(u_char *args_, const struct pcap_pkthdr *info, const u_char *packet) {
    auto *args = reinterpret_cast<pcap_callback_args *>(args_);

    if (info->caplen != info->len) { cs120_warn("packet truncated!"); }

    std::vector<uint8_t> data{packet, packet + info->caplen};

    std::unique_lock guard{*args->lock};
    args->queue->push(std::move(data));
    args->variable->notify_one();
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
RawSocket::RawSocket() : receiver{}, lock{nullptr}, variable{nullptr}, receive_queue{nullptr} {
    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;

    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    pcap_t *pcap_handle = pcap_open_live(device->name, SOCKET_BUFFER_SIZE, 0, 1, pcap_error);
    if (pcap_handle == nullptr) { cs120_abort(pcap_error); }

    pcap_freealldevs(device);

    receive_queue = new std::queue<std::vector<uint8_t>>{};
    variable = new std::condition_variable{};
    lock = new std::mutex{};

    auto *args = new pcap_callback_args{
            .pcap_handle = pcap_handle,
            .filter = (struct bpf_program) {},
            .lock = lock,
            .variable = variable,
            .queue = receive_queue,
    };

    if (pcap_compile(pcap_handle, &args->filter, "", 0, PCAP_NETMASK_UNKNOWN) == PCAP_ERROR) {
        cs120_abort(pcap_geterr(pcap_handle));
    }

    pthread_create(&receiver, nullptr, raw_socket_receiver, args);
}

std::vector<uint8_t> RawSocket::recv() {
    std::unique_lock guard{*lock};

    while (receive_queue->empty()) {
        variable->wait(guard);
    }

    auto item = receive_queue->front();
    receive_queue->pop();
    return item;
}
}
