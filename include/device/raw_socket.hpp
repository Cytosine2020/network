#ifndef CS120_RAW_SOCKET_HPP
#define CS120_RAW_SOCKET_HPP

#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>
#include "pthread.h"

#include "pcap/pcap.h"

#include "utility.hpp"
#include "wire.hpp"


namespace cs120 {
static constexpr size_t SOCKET_BUFFER_SIZE = 2048;

class RawSocket {
private:
    pthread_t receiver;
    std::mutex *lock;
    std::condition_variable *variable;
    std::queue<std::vector<uint8_t>> *receive_queue;

public:
    RawSocket();

    RawSocket(RawSocket &&other) noexcept = default;

    RawSocket &operator=(RawSocket &&other) noexcept = default;

    std::vector<uint8_t> recv();

    ~RawSocket() = default;
};
}


#endif //CS120_RAW_SOCKET_HPP
