#include "queue.hpp"


namespace cs120 {
SPSCQueueSenderSlotGuard SPSCQueue::try_send() {
    sender_lock.lock();

    if (index_increase(end) == start) {
        sender_lock.unlock();
        return SPSCQueueSenderSlotGuard{MutSlice<uint8_t>{}, *this};
    } else {
        return SPSCQueueSenderSlotGuard{get_slot(end), *this};
    }
}

void SPSCQueue::commit() {
    end.store(index_increase(end), std::memory_order_seq_cst);
    sender_lock.unlock();

    std::unique_lock guard{lock};
    variable.notify_one();
}


SPSCQueueReceiverSlotGuard SPSCQueue::recv() {
    receiver_lock.lock();

    if (start == end) {
        std::unique_lock guard{lock};

        while (start == end) {
            variable.wait(guard);
        }
    }

    return SPSCQueueReceiverSlotGuard{get_slot(start), *this};
}

void SPSCQueue::claim() {
    start.store(index_increase(start), std::memory_order_seq_cst);
    receiver_lock.unlock();
}
}
