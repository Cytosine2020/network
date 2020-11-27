#ifndef CS120_RAW_SOCKET_HPP
#define CS120_RAW_SOCKET_HPP


#include "pthread.h"

#include "pcap/pcap.h"

#include "utility.hpp"
#include "wire.hpp"


namespace cs120 {
class RawSocket {
private:
    static constexpr size_t PCAP_BUFFER_SIZE = 4096;

    pthread_t receiver;

public:
    RawSocket();

    RawSocket(RawSocket &&other) noexcept = default;

    RawSocket &operator=(RawSocket &&other) noexcept = default;

    ~RawSocket() = default;
};
}


#endif //CS120_RAW_SOCKET_HPP
