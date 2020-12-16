#ifndef CS120_QUEUE_HPP
#define CS120_QUEUE_HPP


#include <queue>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>

#include "utility.hpp"


namespace cs120 {
template<typename T>
class MPSCQueue;

template<typename T>
class MPSCSenderSlotGuard {
private:
    T *inner;
    MPSCQueue<T> &queue;

public:
    MPSCSenderSlotGuard(T *inner, MPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

    MPSCSenderSlotGuard(const MPSCSenderSlotGuard &other) = delete;

    MPSCSenderSlotGuard &operator=(const MPSCSenderSlotGuard &other) = delete;

    bool empty() const { return inner == nullptr; }

    T &operator*() { return *inner; }

    T *operator->() { return inner; }

    ~MPSCSenderSlotGuard();
};

template<typename T>
class MPSCReceiverSlotGuard {
private:
    T *inner;
    MPSCQueue<T> &queue;

public:
    MPSCReceiverSlotGuard(T *inner, MPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

    MPSCReceiverSlotGuard(const MPSCSenderSlotGuard<T> &other) = delete;

    MPSCReceiverSlotGuard &operator=(const MPSCSenderSlotGuard<T> &other) = delete;

    bool empty() const { return inner == nullptr; }

    T &operator*() { return *inner; }

    T *operator->() { return inner; }

    ~MPSCReceiverSlotGuard();
};


template<typename T>
class MPSCSender {
private:
    std::shared_ptr<MPSCQueue<T>> queue;

public:
    MPSCSender(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

    MPSCSender(MPSCSender &&other) noexcept = default;

    MPSCSender &operator=(MPSCSender &&other) noexcept = default;

    MPSCSenderSlotGuard<T> try_send();

    MPSCSenderSlotGuard<T> send();

    ~MPSCSender() = default;
};

template<typename T>
class MPSCReceiver {
private:
    std::shared_ptr<MPSCQueue<T>> queue;

public:
    MPSCReceiver(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

    MPSCReceiver(MPSCReceiver &&other) noexcept = default;

    MPSCReceiver &operator=(MPSCReceiver &&other) noexcept = default;

    MPSCReceiverSlotGuard<T> try_recv();

    MPSCReceiverSlotGuard<T> recv();

    template<typename ClockT, typename DurationT>
    MPSCReceiverSlotGuard<T>
    recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time);

    ~MPSCReceiver() = default;
};


template<typename T>
class MPSCQueue {
private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable empty, full;
    Array<T> inner;
    size_t size;
    std::atomic<size_t> start, end;

    size_t index_increase(size_t index) const { return index + 1 >= size ? 0 : index + 1; }

    MPSCQueue(size_t size) : lock{}, empty{}, full{}, inner{size}, size{size}, start{0}, end{0} {}
public:
    static std::pair<MPSCSender<T>, MPSCReceiver<T>> channel(size_t size) {
        std::shared_ptr<MPSCQueue> queue{new MPSCQueue{size}};
        return std::make_pair(MPSCSender<T>{queue}, MPSCReceiver<T>{queue});
    }

    MPSCQueue(MPSCQueue &other) = delete;

    MPSCQueue &operator=(const MPSCQueue &other) = delete;

    MPSCSenderSlotGuard<T> try_send() {
        sender_lock.lock();

        if (index_increase(end) == start.load()) {
            sender_lock.unlock();
            return MPSCSenderSlotGuard<T>{nullptr, *this};
        } else {
            return MPSCSenderSlotGuard<T>{&inner[end.load()], *this};
        }
    }

    MPSCSenderSlotGuard<T> send() {
        sender_lock.lock();

        if (index_increase(end.load()) == start.load()) {
            std::unique_lock guard{lock};

            while (index_increase(end.load()) == start.load()) {
                full.wait(guard);
            }
        }

        return MPSCSenderSlotGuard{&inner[end.load()], *this};
    }

    void commit() {
        end.store(index_increase(end.load()));
        sender_lock.unlock();

        std::unique_lock guard{lock};
        empty.notify_all();
    }

    MPSCReceiverSlotGuard<T> try_recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            receiver_lock.unlock();
            return MPSCReceiverSlotGuard<T>{nullptr, *this};
        } else {
            return MPSCReceiverSlotGuard<T>{&inner[start.load()], *this};
        }
    }

    MPSCReceiverSlotGuard<T> recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock guard{lock};

            while (start.load() == end.load()) {
                empty.wait(guard);
            }
        }

        return MPSCReceiverSlotGuard<T>{&inner[start.load()], *this};
    }

    template<typename ClockT, typename DurationT>
    MPSCReceiverSlotGuard<T>
    recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock guard{lock};

            while (start.load() == end.load()) {
                if (empty.template wait_until(guard, time) == std::cv_status::timeout) {
                    goto timeout;
                }
            }
        }

        return MPSCReceiverSlotGuard<T>{&inner[start.load()], *this};

        timeout:
        receiver_lock.unlock();
        return MPSCReceiverSlotGuard<T>{nullptr, *this};
    }

    void claim() {
        start.store(index_increase(start.load()));
        receiver_lock.unlock();

        std::unique_lock guard{lock};
        full.notify_all();
    }

    ~MPSCQueue() = default;
};

template<typename T>
MPSCSenderSlotGuard<T>::~MPSCSenderSlotGuard() {
    if (!empty()) { queue.commit(); }
}

template<typename T>
MPSCReceiverSlotGuard<T>::~MPSCReceiverSlotGuard() {
    if (!empty()) { queue.claim(); }
}

template<typename T>
MPSCSenderSlotGuard<T> MPSCSender<T>::try_send() { return queue->try_send(); }

template<typename T>
MPSCSenderSlotGuard<T> MPSCSender<T>::send() { return queue->send(); }

template<typename T>
MPSCReceiverSlotGuard<T> MPSCReceiver<T>::try_recv() { return queue->try_recv(); }

template<typename T>
MPSCReceiverSlotGuard<T> MPSCReceiver<T>::recv() { return queue->recv(); }

template<typename T>
template<typename ClockT, typename DurationT>
MPSCReceiverSlotGuard<T>
MPSCReceiver<T>::recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
    return queue->recv_deadline(time);
}
}


#endif //CS120_QUEUE_HPP
