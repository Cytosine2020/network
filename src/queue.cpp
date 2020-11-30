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

SPSCQueueSenderSlotGuard SPSCQueue::send() {
    sender_lock.lock();

    if (index_increase(end.load()) == start.load()) {
        std::unique_lock guard{lock};

        while (index_increase(end.load()) == start.load()) {
            variable.wait(guard);
        }
    }

    return SPSCQueueSenderSlotGuard{get_slot(end.load()), *this};
}

void SPSCQueue::commit() {
    end.store(index_increase(end.load()));
    sender_lock.unlock();

    std::unique_lock guard{lock};
    variable.notify_all();
}

SPSCQueueReceiverSlotGuard SPSCQueue::try_recv() {
    receiver_lock.lock();

    if (start.load() == end.load()) {
        receiver_lock.unlock();
        return SPSCQueueReceiverSlotGuard{MutSlice<uint8_t>{}, *this};
    } else {
        return SPSCQueueReceiverSlotGuard{get_slot(start.load()), *this};
    }
}

SPSCQueueReceiverSlotGuard SPSCQueue::recv() {
    receiver_lock.lock();

    if (start.load() == end.load()) {
        std::unique_lock guard{lock};

        while (start.load() == end.load()) {
            variable.wait(guard);
        }
    }

    return SPSCQueueReceiverSlotGuard{get_slot(start.load()), *this};
}

void SPSCQueue::claim() {
    start.store(index_increase(start.load()));
    receiver_lock.unlock();

    std::unique_lock guard{lock};
    variable.notify_all();
}
}
