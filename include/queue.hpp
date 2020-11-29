#ifndef CS120_QUEUE_HPP
#define CS120_QUEUE_HPP


#include <queue>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "utility.hpp"


namespace cs120 {
class SPSCQueue;

class SPSCQueueSenderSlotGuard {
private:
    MutSlice<uint8_t> inner;
    SPSCQueue &queue;

public:
    SPSCQueueSenderSlotGuard(MutSlice<uint8_t> inner, SPSCQueue &queue) :
            inner{inner}, queue{queue} {}

    SPSCQueueSenderSlotGuard(const SPSCQueueSenderSlotGuard &other) = delete;

    SPSCQueueSenderSlotGuard &operator=(const SPSCQueueSenderSlotGuard &other) = delete;

    MutSlice<uint8_t> &operator*() { return inner; }

    MutSlice<uint8_t> *operator->() { return &inner; }

    ~SPSCQueueSenderSlotGuard();
};

class SPSCQueueReceiverSlotGuard {
private:
    MutSlice<uint8_t> inner;
    SPSCQueue &queue;

public:
    SPSCQueueReceiverSlotGuard(MutSlice<uint8_t> inner, SPSCQueue &queue) :
            inner{inner}, queue{queue} {}

    SPSCQueueReceiverSlotGuard(const SPSCQueueSenderSlotGuard &other) = delete;

    SPSCQueueReceiverSlotGuard &operator=(const SPSCQueueSenderSlotGuard &other) = delete;

    MutSlice<uint8_t> &operator*() { return inner; }

    MutSlice<uint8_t> *operator->() { return &inner; }

    ~SPSCQueueReceiverSlotGuard();
};


class SPSCQueue {
private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable variable;
    Array<uint8_t> inner;
    size_t buffer_size, size;
    std::atomic<size_t> start, end;

    size_t index_increase(size_t index) const { return (index + 1) % size; }

    MutSlice<uint8_t> get_slot(size_t index) {
        return inner[Range{index * buffer_size}][Range{0, buffer_size}];
    }

public:
    SPSCQueue(size_t buffer_size, size_t size) :
            lock{}, variable{}, inner{buffer_size * size},
            buffer_size{buffer_size}, size{size}, start{0}, end{0} {}

    SPSCQueue(const SPSCQueue &other) = delete;

    SPSCQueue &operator=(const SPSCQueue &other) = delete;

    SPSCQueueSenderSlotGuard send();

    SPSCQueueSenderSlotGuard try_send();

    void commit();

    SPSCQueueReceiverSlotGuard try_recv();

    SPSCQueueReceiverSlotGuard recv();

    void claim();

    ~SPSCQueue() = default;
};

inline SPSCQueueSenderSlotGuard::~SPSCQueueSenderSlotGuard() {
    if (!inner.empty()) { queue.commit(); }
}

inline SPSCQueueReceiverSlotGuard::~SPSCQueueReceiverSlotGuard() {
    if (!inner.empty()) { queue.claim(); }
}
}


#endif //CS120_QUEUE_HPP
