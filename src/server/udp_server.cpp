#include "server/udp_server.hpp"

#include "wire/wire.hpp"
#include "wire/ipv4.hpp"
#include "wire/udp.hpp"


namespace cs120 {
UDPServer::UDPServer(std::shared_ptr<BaseSocket> &device, size_t size,
                     uint32_t src_ip, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port) :
        device{device}, send_queue{}, recv_queue{},
        src_ip{src_ip}, dest_ip{dest_ip}, src_port{src_port}, dest_port{dest_port},
        receive_buffer{device->get_mtu()},
        receive_buffer_slice{}, identifier{1} {
    auto[send, recv] = device->bind([=](auto ip_header, auto ip_option, auto ip_data) {
        (void) ip_option;

        if (ip_header->get_protocol() != IPV4Protocol::UDP ||
            ip_header->get_src_ip() != dest_ip ||
            ip_header->get_dest_ip() != src_ip) { return false; }

        auto[udp_header, udp_data] = udp_split(ip_data);
        if (udp_header == nullptr) {
            cs120_warn("invalid package!");
            return false;
        }

        if (udp_header->get_src_port() != dest_port ||
            udp_header->get_dest_port() != src_port) { return false; }

        return true;
    }, size);

    send_queue = std::move(send);
    recv_queue = std::move(recv);
}

size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    size_t maximum = UDPHeader::max_payload(device->get_mtu());

    while (!data.empty()) {
        auto buffer = send_queue.send();

        size_t size = std::min(maximum, data.size());

        {
            UDPHeader::generate((*buffer)[Range{}], identifier, src_ip, dest_ip,
                                src_port, dest_port, size)
                    ->copy_from_slice(data[Range{0, size}]);
        }

        data = data[Range{size}];

        ++identifier;
    }

    return length;
}

size_t UDPServer::recv(MutSlice<uint8_t> data) {
    auto last_size = receive_buffer_slice.size();

    if (last_size > 0) {
        if (last_size > data.size()) {
            data.copy_from_slice(receive_buffer_slice[Range{0, data.size()}]);
            receive_buffer_slice = receive_buffer_slice[Range{data.size()}];
            return data.size();
        } else {
            data[Range{0, last_size}].copy_from_slice(receive_buffer_slice);
            receive_buffer_slice = MutSlice<uint8_t>{};
            return last_size;
        }
    }

    while (!data.empty()) {
        auto buffer = recv_queue->recv();

        auto *ip_header = buffer->buffer_cast<IPV4Header>();
        if (ip_header == nullptr || complement_checksum(ip_header->into_slice()) != 0) {
            cs120_warn("invalid package!");
            continue;
        }

        auto ip_data = (*buffer)[Range{ip_header->get_header_length(),
                                       ip_header->get_total_length()}];

        auto[udp_header, udp_data] = udp_split(ip_data);
        if (udp_header == nullptr ||
            !udp_header->check_checksum(complement_checksum(*ip_header, ip_data))) {
            cs120_warn("invalid package!");
            continue;
        }

        if (udp_data.size() > data.size()) {
            data.copy_from_slice(udp_data[Range{0, data.size()}]);
            receive_buffer_slice = receive_buffer[Range{0, udp_data.size() - data.size()}];
            receive_buffer_slice.copy_from_slice(udp_data[Range{data.size()}]);
            return data.size();
        } else {
            data[Range{0, udp_data.size()}].copy_from_slice(udp_data);
            return udp_data.size();
        }
    }

    return 0;
}
}
