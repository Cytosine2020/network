#ifndef CS120_QUEUE_HPP
#define CS120_QUEUE_HPP


#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "utility.hpp"


namespace cs120 {
class SPSCQueue {
private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable variable;
    Array<uint8_t> inner;
    size_t buffer_size, size;
    size_t start, end;

    size_t index_increase(size_t index) { return (index + 1) % size; }

    MutSlice<uint8_t> get_slot(size_t index) {
        return inner[Range{index * buffer_size, (index + 1) * buffer_size}];
    }

public:
    SPSCQueue(size_t buffer_size, size_t size) :
            lock{}, variable{}, inner{buffer_size * size},
            buffer_size{buffer_size}, size{size}, start{0}, end{0} {}

    SPSCQueue(const SPSCQueue &other) = delete;

    SPSCQueue &operator=(const SPSCQueue &other) = delete;

    bool send(Slice<uint8_t> item);

    void commit();

    Array<uint8_t> recv();

    void claim();

    ~SPSCQueue() = delete;
};
}


#endif //CS120_QUEUE_HPP
