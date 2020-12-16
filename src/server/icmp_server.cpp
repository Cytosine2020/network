#include "server/icmp_server.hpp"

#include <unistd.h>
#include <chrono>

#include "utility.hpp"
#include "wire/ipv4.hpp"
#include "wire/icmp.hpp"


namespace cs120 {
void ICMPServer::icmp_receiver() {
    for (;;) {
        auto buffer = device->recv();

        auto[ip_header, ip_option, ip_data] = ipv4_split((*buffer)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        if (ip_header->get_protocol() != IPV4Protocol::ICMP) { continue; }

        auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        switch (icmp_header->get_type()) {
            case ICMPType::EchoReply: {
                if (icmp_header->get_code() != 0) { continue; }

                if (ip_header->get_total_length() > BUFFER_SIZE) {
                    cs120_warn("package too big!");
                    continue;
                }

                auto send = ping_queue.try_send();
                if (!send.empty()) {
                    auto range = Range{0, ip_header->get_total_length()};
                    (*send)[range].copy_from_slice((*buffer)[range]);
                }
                break;
            }
            case ICMPType::EchoRequest: {
                if (icmp_header->get_code() != 0) { continue; }

                auto send = device->try_send();
                if (send.empty()) {
                    cs120_warn("echo reply loss!");
                } else {
                    icmp_header->set_type(ICMPType::EchoReply);
                    auto range = Range{0, ip_header->get_total_length()};
                    (*send)[range].copy_from_slice((*buffer)[range]);
                }
            }
                break;
            default:
                break;
        }
    }
}


ICMPServer::ICMPServer(std::unique_ptr<BaseSocket> device, uint16_t identification) :
        device{std::move(device)}, receiver{}, ping_queue{64}, identification{identification} {
    pthread_create(&receiver, nullptr, icmp_receiver, this);
}

bool ICMPServer::ping(uint16_t seq, uint32_t src_ip, uint32_t dest_ip,
                      uint16_t src_port, uint16_t dest_port) {
    using namespace std::literals;

    while (!ping_queue.try_recv().empty()) {}

    ICMPData send_port{src_port, dest_port};
    Slice<uint8_t> data{reinterpret_cast<uint8_t *>(&send_port), sizeof(ICMPData)};

    {
        auto buffer = device->send();
        if (ICMPHeader::generate((*buffer)[Range{}], seq + 1, src_ip, dest_ip,
                                 ICMPType::EchoRequest, identification, seq, data) == 0) {
            cs120_abort("header generation failed!");
        }
    }

    auto deadline = std::chrono::system_clock::now() + 1s;

    for (;;) {
        auto recv = ping_queue.recv_deadline(deadline);
        if (recv.empty()) { return false; }

        auto[ip_header, ip_option, ip_data] = ipv4_split((*recv)[Range{}]);
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        if (ip_header->get_protocol() != IPV4Protocol::ICMP ||
            ip_header->get_dest_ip() != src_ip) { continue; }

        auto *icmp_header = ip_data.buffer_cast<ICMPHeader>();
        if (icmp_header == nullptr || complement_checksum(ip_data) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        if (icmp_header->get_type() != ICMPType::EchoReply ||
            icmp_header->get_code() != 0 ||
            icmp_header->get_identification() != identification ||
            icmp_header->get_sequence() != seq) { continue; }

        auto icmp_data = ip_data[Range{sizeof(ICMPHeader)}];
        if (icmp_data.size() < sizeof(ICMPData)) { continue; }
        auto *recv_port = reinterpret_cast<struct ICMPData *>(icmp_data.begin());
        if (recv_port->src_port != src_port || recv_port->dest_port != dest_port) { continue; }

        break;
    }

    return true;
}
}
