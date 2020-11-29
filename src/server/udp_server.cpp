#include "server/udp_server.hpp"

#include "wire.hpp"


namespace cs120 {
size_t UDPServer::send(Slice<uint8_t> data) {
    size_t length = data.size();

    size_t max=udp_max_payload(device->get_mtu());



    while (!data.empty()) {
        auto buffer=device->send();
        if (max<length){
            buffer->copy_from_slice(data[Range{0,max}]);
            data=data[Range{max}];
            length-=max;
        }else{
            (*buffer)[Range{0,length}].copy_from_slice(data);
            data=data[Range{length}];
        }

    }

    return length;
}

size_t UDPServer::recv(MutSlice<uint8_t> data) {
    cs120_abort("unimplemented!");
}
}
