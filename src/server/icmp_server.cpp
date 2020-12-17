#include "server/icmp_server.hpp"

#include <unistd.h>
#include <chrono>

#include "utility.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"
#include "device/base_socket.hpp"


namespace cs120 {
bool ICMPPing::ping(uint16_t seq) {
    using namespace std::literals;

    ICMPData send_port{src_port, dest_port};
    Slice<uint8_t> data{reinterpret_cast<uint8_t *>(&send_port), sizeof(ICMPData)};

    {
        auto buffer = send_queue.send();
        if (ICMPHeader::generate((*buffer)[Range{}], seq + 1, src_ip, dest_ip,
                                 ICMPType::EchoRequest, identification, seq, data) == 0) {
            cs120_abort("header generation failed!");
        }
    }

    auto deadline = std::chrono::system_clock::now() + 1s;

    for (;;) {
        auto buffer = recv_queue->recv_deadline(deadline);
        if (buffer.empty()) { return false; }

        auto[ip_header, ip_option, ip_data] = ipv4_split((*buffer)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        if (icmp_header->get_sequence() != seq) { continue; }

        break;
    }

    return true;
}

void ICMPServer::icmp_receiver() {
    for (;;) {
        auto buffer = recv_queue->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*buffer)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto send = send_queue.try_send();
        if (send.empty()) {
            cs120_warn("echo reply loss!");
        } else {
            icmp_header->set_type(ICMPType::EchoReply);
            icmp_header->set_checksum(complement_checksum(ip_data));
            auto range = Range{0, ip_header->get_total_length()};
            (*send)[range].copy_from_slice((*buffer)[range]);
        }
    }
}


ICMPServer::ICMPServer(std::shared_ptr<BaseSocket> device, uint32_t ip_addr) :
        device{std::move(device)}, receiver{}, send_queue{}, recv_queue{}, ip_addr{ip_addr} {
    auto[send, recv] = this->device->bind([=](auto *ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;

        if (ip_header->get_protocol() != IPV4Protocol::ICMP ||
            ip_header->get_dest_ip() != ip_addr) { return false; }

        auto *icmp_header = ip_data.template buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (icmp_header->get_type() != ICMPType::EchoRequest ||
            icmp_header->get_code() != 0) { return false; }

        return true;
    }, 64);

    send_queue = std::move(send);
    recv_queue = std::move(recv);

    pthread_create(&receiver, nullptr, icmp_receiver, this);
}

ICMPPing
ICMPServer::create_ping(uint16_t identification, uint32_t dest_ip,
                        uint16_t src_port, uint16_t dest_port) {
    auto[send, recv] = this->device->bind([=](auto *ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;

        if (ip_header->get_protocol() != IPV4Protocol::ICMP ||
            ip_header->get_dest_ip() != ip_addr) { return false; }

        auto *icmp_header = ip_data.template buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (icmp_header->get_type() != ICMPType::EchoReply ||
            icmp_header->get_code() != 0 ||
            icmp_header->get_identification() != identification) { return false; }

        auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
        if (icmp_data.size() < sizeof(ICMPData)) { return false; }
        auto *recv_port = reinterpret_cast<const ICMPData *>(icmp_data.begin());
        if (recv_port->src_port != src_port || recv_port->dest_port != dest_port) { return false; }

        return true;
    }, 64);

    return ICMPPing(std::move(send), std::move(recv), ip_addr, dest_ip,
                    src_port, dest_port, identification);
}
}
