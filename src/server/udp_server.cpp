#include "server/udp_server.hpp"

#include "wire/wire.hpp"
#include "wire/ipv4.hpp"
#include "wire/udp.hpp"


namespace cs120 {
size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    size_t maximum = UDPHeader::max_payload(device->get_mtu());

    while (!data.empty()) {
        auto buffer = device->send();

        size_t size = std::min(maximum, data.size());

        UDPHeader::generate(*buffer, identifier, src_ip, dest_ip, src_port, dest_port,
                            data[Range{0, size}]);

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
            receive_buffer_slice = MutSlice<uint8_t>{receive_buffer.begin(), 0};
            return last_size;
        }
    }

    while (!data.empty()) {
        auto buffer = device->recv();

        auto *ip_header = buffer->buffer_cast<IPV4Header>();
        if (ip_header == nullptr) { continue; }

        if (ip_header->get_protocol() != IPV4Header::UDP) { continue; }

        if (ip_header->get_source_ip() != src_ip || ip_header->get_destination_ip() != dest_ip) {
            continue;
        }

        auto ip_data = (*buffer)[Range{ip_header->get_header_length(),
                                       ip_header->get_total_length()}];

        auto *udp_header = ip_data.buffer_cast<UDPHeader>();
        if (udp_header == nullptr) { continue; }

        if (udp_header->get_destination_port() != src_port ||
            udp_header->get_source_port() != dest_port) { continue; }

        auto udp_data = ip_data[Range{udp_header->get_header_length(), udp_header->get_total_length()}];

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
