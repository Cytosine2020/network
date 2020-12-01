#include "server/udp_server.hpp"

#include "wire.hpp"


namespace cs120 {
size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    size_t maximum = udp_max_payload(device->get_mtu());

    while (!data.empty()) {
        auto buffer = device->send();

        size_t size = std::min(maximum, data.size());

        generate_udp(*buffer, identifier, src_ip, dest_ip, src_port, dest_port,
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

        size_t total_length = get_ipv4_total_size(*buffer);

        if (total_length == 0) { continue; }

        auto *ip_header = buffer->buffer_cast<struct ip>();

        size_t header_length = ip_get_ihl(*ip_header);
        if (header_length > total_length) { continue; }

        if (ip_get_protocol(*ip_header) != IPPROTO_UDP) { continue; }

        if (ip_get_daddr(*ip_header) != src_ip || ip_get_saddr(*ip_header) != dest_ip) {
            continue;
        }

        auto ip_data = (*buffer)[Range{header_length, total_length}];

        auto *udp_header = ip_data.buffer_cast<struct udphdr>();

        if (udp_header == nullptr) { continue; }

        if (udphdr_get_dest(*udp_header) != src_port ||
            udphdr_get_source(*udp_header) != dest_port) { continue; }

        auto udp_size = udphdr_get_len(*udp_header);

        if (udp_size < sizeof(struct udphdr) || udp_size > ip_data.size()) { continue; }

        auto udp_data = ip_data[Range{0, udp_size}][Range{sizeof(struct udphdr)}];

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
