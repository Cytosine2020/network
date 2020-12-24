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
class MPSCQueue {
public:
    class SenderSlotGuard {
    private:
        T *inner;
        MPSCQueue<T> &queue;

    public:
        SenderSlotGuard(T *inner, MPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

        SenderSlotGuard(SenderSlotGuard &&other) noexcept = default;

        SenderSlotGuard &operator=(SenderSlotGuard &&other) noexcept = default;

        bool empty() const { return inner == nullptr; }

        T &operator*() { return *inner; }

        T *operator->() { return inner; }

        ~SenderSlotGuard() { if (!empty()) { queue.commit(); }}
    };


    class ReceiverSlotGuard {
    private:
        T *inner;
        MPSCQueue<T> &queue;

    public:
        ReceiverSlotGuard(T *inner, MPSCQueue<T> &queue) : inner{inner}, queue{queue} {}

        ReceiverSlotGuard(ReceiverSlotGuard &&other) noexcept = default;

        ReceiverSlotGuard &operator=(ReceiverSlotGuard &&other) noexcept = default;

        bool empty() const { return inner == nullptr; }

        T &operator*() { return *inner; }

        T *operator->() { return inner; }

        ~ReceiverSlotGuard() { if (!empty()) { queue.claim(); }}
    };


    class Sender {
    private:
        std::shared_ptr<MPSCQueue<T>> queue;

    public:
        Sender() : queue{nullptr} {}

        explicit Sender(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

        Sender(const Sender &other) = default;

        Sender &operator=(const Sender &other) = default;

        Sender(Sender &&other) noexcept = default;

        Sender &operator=(Sender &&other) noexcept = default;

        SenderSlotGuard try_send() { return queue->try_send(); }

        SenderSlotGuard send() { return queue->send(); }

        ~Sender() = default;
    };


    class Receiver {
    private:
        std::shared_ptr<MPSCQueue<T>> queue;

    public:
        Receiver() : queue{nullptr} {}

        explicit Receiver(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

        Receiver(Receiver &&other) noexcept = default;

        Receiver &operator=(Receiver &&other) noexcept = default;

        ReceiverSlotGuard try_recv() { return queue->try_recv(); }

        ReceiverSlotGuard recv() { return queue->recv(); }

        template<typename ClockT, typename DurationT>
        ReceiverSlotGuard recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
            return queue->recv_deadline(time);
        }

        ~Receiver() = default;
    };

private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable empty, full;
    Array<T> inner;
    size_t size;
    std::atomic<size_t> start, end;

    size_t index_increase(size_t index) const { return index + 1 >= size ? 0 : index + 1; }

    explicit MPSCQueue(size_t size) :
            lock{}, empty{}, full{}, inner{size}, size{size}, start{0}, end{0} {}

public:
    static std::pair<Sender, Receiver> channel(size_t size) {
        std::shared_ptr<MPSCQueue> queue{new MPSCQueue{size}};
        return std::make_pair(Sender{queue}, Receiver{queue});
    }

    MPSCQueue(MPSCQueue &other) = delete;

    MPSCQueue &operator=(const MPSCQueue &other) = delete;

    SenderSlotGuard try_send() {
        sender_lock.lock();

        if (index_increase(end) == start.load()) {
            sender_lock.unlock();
            return SenderSlotGuard{nullptr, *this};
        } else {
            return SenderSlotGuard{&inner[end.load()], *this};
        }
    }

    SenderSlotGuard send() {
        sender_lock.lock();

        if (index_increase(end.load()) == start.load()) {
            std::unique_lock<std::mutex> guard{lock};

            while (index_increase(end.load()) == start.load()) {
                full.wait(guard);
            }
        }

        return SenderSlotGuard{&inner[end.load()], *this};
    }

    void commit() {
        end.store(index_increase(end.load()));
        sender_lock.unlock();

        std::unique_lock<std::mutex> guard{lock};
        empty.notify_one();
    }

    ReceiverSlotGuard try_recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            receiver_lock.unlock();
            return ReceiverSlotGuard{nullptr, *this};
        } else {
            return ReceiverSlotGuard{&inner[start.load()], *this};
        }
    }

    ReceiverSlotGuard recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock<std::mutex> guard{lock};

            while (start.load() == end.load()) {
                empty.wait(guard);
            }
        }

        return ReceiverSlotGuard{&inner[start.load()], *this};
    }

    template<typename ClockT, typename DurationT>
    ReceiverSlotGuard recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            std::unique_lock<std::mutex> guard{lock};

            while (start.load() == end.load()) {
                if (empty.template wait_until(guard, time) == std::cv_status::timeout) {
                    goto timeout;
                }
            }
        }

        return ReceiverSlotGuard{&inner[start.load()], *this};

        timeout:
        receiver_lock.unlock();
        return ReceiverSlotGuard{nullptr, *this};
    }

    void claim() {
        clear<T>::inner(inner[start.load()]);

        start.store(index_increase(start.load()));
        receiver_lock.unlock();

        std::unique_lock<std::mutex> guard{lock};
        full.notify_one();
    }

    ~MPSCQueue() = default;
};
}


#endif //CS120_QUEUE_HPP
