#ifndef CS120_QUEUE_HPP
#define CS120_QUEUE_HPP


#include <queue>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "utility.hpp"


namespace cs120 {
template<typename T>
class SPSCQueue;

template<typename T>
class SPSCQueueSenderSlotGuard {
private:
    T *inner;
    SPSCQueue<T> &queue;

public:
    SPSCQueueSenderSlotGuard(T *inner, SPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

    SPSCQueueSenderSlotGuard(const SPSCQueueSenderSlotGuard &other) = delete;

    SPSCQueueSenderSlotGuard &operator=(const SPSCQueueSenderSlotGuard &other) = delete;

    bool empty() const { return inner == nullptr; }

    T &operator*() { return *inner; }

    T *operator->() { return inner; }

    ~SPSCQueueSenderSlotGuard();
};

template<typename T>
class SPSCQueueReceiverSlotGuard {
private:
    T *inner;
    SPSCQueue<T> &queue;

public:
    SPSCQueueReceiverSlotGuard(T *inner, SPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

    SPSCQueueReceiverSlotGuard(const SPSCQueueSenderSlotGuard<T> &other) = delete;

    SPSCQueueReceiverSlotGuard &operator=(const SPSCQueueSenderSlotGuard<T> &other) = delete;

    bool empty() const { return inner == nullptr; }

    T &operator*() { return *inner; }

    T *operator->() { return inner; }

    ~SPSCQueueReceiverSlotGuard();
};


template<typename T>
class SPSCQueue {
private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable empty, full;
    Array<T> inner;
    size_t size;
    std::atomic<size_t> start, end;

    size_t index_increase(size_t index) const { return index + 1 >= size ? 0 : index + 1; }

public:
    SPSCQueue(size_t size) : lock{}, empty{}, full{}, inner{size}, size{size}, start{0}, end{0} {}

    SPSCQueue(SPSCQueue &other) = delete;

    SPSCQueue &operator=(const SPSCQueue &other) = delete;

    SPSCQueueSenderSlotGuard<T> try_send() {
        sender_lock.lock();

        if (index_increase(end) == start.load()) {
            sender_lock.unlock();
            return SPSCQueueSenderSlotGuard<T>{nullptr, *this};
        } else {
            return SPSCQueueSenderSlotGuard<T>{&inner[end.load()], *this};
        }
    }

    SPSCQueueSenderSlotGuard<T> send() {
        sender_lock.lock();

        if (index_increase(end.load()) == start.load()) {
            std::unique_lock guard{lock};

            while (index_increase(end.load()) == start.load()) {
                full.wait(guard);
            }
        }

        return SPSCQueueSenderSlotGuard{&inner[end.load()], *this};
    }

    void commit() {
        end.store(index_increase(end.load()));
        sender_lock.unlock();

        std::unique_lock guard{lock};
        empty.notify_all();
    }

    SPSCQueueReceiverSlotGuard<T> try_recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            receiver_lock.unlock();
            return SPSCQueueReceiverSlotGuard<T>{nullptr, *this};
        } else {
            return SPSCQueueReceiverSlotGuard<T>{&inner[start.load()], *this};
        }
    }

    SPSCQueueReceiverSlotGuard<T> recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock guard{lock};

            while (start.load() == end.load()) {
                empty.wait(guard);
            }
        }

        return SPSCQueueReceiverSlotGuard<T>{&inner[start.load()], *this};
    }

    template<typename ClockT, typename DurationT>
    SPSCQueueReceiverSlotGuard<T> recv_deadline(
            const std::chrono::time_point<ClockT, DurationT> &time
    ) {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock guard{lock};

            while (start.load() == end.load()) {
                if (empty.template wait_until(guard, time) == std::cv_status::timeout) {
                    goto timeout;
                }
            }
        }

        return SPSCQueueReceiverSlotGuard<T>{&inner[start.load()], *this};

        timeout:
        receiver_lock.unlock();
        return SPSCQueueReceiverSlotGuard<T>{nullptr, *this};
    }

    void claim() {
        start.store(index_increase(start.load()));
        receiver_lock.unlock();

        std::unique_lock guard{lock};
        full.notify_all();
    }

    ~SPSCQueue() = default;
};

template<typename T>
inline SPSCQueueSenderSlotGuard<T>::~SPSCQueueSenderSlotGuard() {
    if (!empty()) { queue.commit(); }
}

template<typename T>
inline SPSCQueueReceiverSlotGuard<T>::~SPSCQueueReceiverSlotGuard() {
    if (!empty()) { queue.claim(); }
}
}


#endif //CS120_QUEUE_HPP
