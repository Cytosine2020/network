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
    enum class Error {
        None,
        Empty,
        Closed,
    };

    class SenderSlotGuard {
    private:
        T *inner;
        MPSCQueue<T> *queue;
        Error error;

    public:
        explicit SenderSlotGuard(Error error): inner{nullptr}, queue{nullptr}, error{error} {}

        SenderSlotGuard(T *inner, MPSCQueue<T> *queue) :
                inner{inner}, queue{queue}, error{Error::None} {}

        SenderSlotGuard(SenderSlotGuard &&other) noexcept {
            if (this != &other) {
                this->inner = other.inner;
                this->queue = other.queue;
                other.inner = nullptr;
                other.queue = nullptr;
            }
        }

        SenderSlotGuard &operator=(SenderSlotGuard &&other) noexcept {
            if (this != &other) {
                if (!this->none()) { queue->commit(); }
                this->inner = other.inner;
                this->queue = other.queue;
                other.inner = nullptr;
                other.queue = nullptr;
            }

            return *this;
        }

        bool none() const { return inner == nullptr; }

        Error get_error() const { return error; }

        bool is_empty() const { return error == Error::Empty; }

        bool is_close() const { return error == Error::Closed; }

        SenderSlotGuard unwrap() {
            if (is_close()) { cs120_abort("channel closed unexpectedly!"); }

            return std::move(*this);
        }

        T &operator*() { return *inner; }

        T *operator->() { return inner; }

        ~SenderSlotGuard() { if (!none()) { queue->commit(); }}
    };


    class ReceiverSlotGuard {
    private:
        T *inner;
        MPSCQueue<T> *queue;
        Error error;

    public:
        explicit ReceiverSlotGuard(Error error): inner{nullptr}, queue{nullptr}, error{error} {}

        ReceiverSlotGuard(T *inner, MPSCQueue<T> *queue) :
                inner{inner}, queue{queue}, error{Error::None} {}

        ReceiverSlotGuard(ReceiverSlotGuard &&other) noexcept {
            if (this != &other) {
                this->inner = other.inner;
                this->queue = other.queue;
                other.inner = nullptr;
                other.queue = nullptr;
            }
        }

        ReceiverSlotGuard &operator=(ReceiverSlotGuard &&other) noexcept {
            if (this != &other) {
                if (!this->none()) { queue->claim(); }
                this->inner = other.inner;
                this->queue = other.queue;
                other.inner = nullptr;
                other.queue = nullptr;
            }

            return *this;
        }

        bool none() const { return inner == nullptr; }

        Error get_error() const { return error; }

        bool is_empty() const { return error == Error::Empty; }

        bool is_close() const { return error == Error::Closed; }

        ReceiverSlotGuard unwrap() {
            if (is_close()) { cs120_abort("channel closed unexpectedly!"); }

            return std::move(*this);
        }

        T &operator*() { return *inner; }

        T *operator->() { return inner; }

        ~ReceiverSlotGuard() { if (!none()) { queue->claim(); }}
    };


    class Sender {
    private:
        std::shared_ptr<MPSCQueue<T>> queue;

    public:
        Sender() noexcept: queue{nullptr} {}

        explicit Sender(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

        Sender(const Sender &other) : queue{other.queue} {
            if (this != &other) { queue->add_sender(); }
        }

        Sender &operator=(const Sender &other) {
            this->queue = other.queue;
            if (this != &other) { queue->add_sender(); }
            return *this;
        }

        Sender(Sender &&other) noexcept = default;

        Sender &operator=(Sender &&other) noexcept = default;

        size_t is_closed() const { return queue->receiver_count() == 0; }

        SenderSlotGuard try_send() { return queue->try_send(); }

        SenderSlotGuard send() { return queue->send(); }

        ~Sender() { if (queue != nullptr) { queue->remove_sender(); }}
    };


    class Receiver {
    private:
        std::shared_ptr<MPSCQueue<T>> queue;

    public:
        Receiver() noexcept: queue{nullptr} {}

        explicit Receiver(std::shared_ptr<MPSCQueue<T>> queue) : queue{queue} {}

        Receiver(Receiver &&other) noexcept = default;

        Receiver &operator=(Receiver &&other) noexcept = default;

        size_t is_closed() const { return queue->sender_count() == 0; }

        ReceiverSlotGuard try_recv() { return queue->try_recv(); }

        ReceiverSlotGuard recv() { return queue->recv(); }

        template<class RepT, class PeriodT>
        ReceiverSlotGuard recv_timeout(const std::chrono::duration<RepT, PeriodT> &period) {
            return queue->recv_timeout(period);
        }

        template<typename ClockT, typename DurationT>
        ReceiverSlotGuard recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
            return queue->recv_deadline(time);
        }

        ~Receiver() { if (queue != nullptr) { queue->remove_receiver(); }}
    };

private:
    std::mutex lock, sender_lock, receiver_lock;
    std::condition_variable empty, full;
    std::atomic<size_t> sender, receiver;
    Array<T> inner;
    size_t size;
    std::atomic<size_t> start, end;

    size_t index_increase(size_t index) const { return index + 1 >= size ? 0 : index + 1; }

    explicit MPSCQueue(size_t size) :
            lock{}, sender_lock{}, receiver_lock{}, empty{}, full{},
            sender{1}, receiver{1}, inner{size}, size{size}, start{0}, end{0} {}

public:
    static std::pair<Sender, Receiver> channel(size_t size) {
        std::shared_ptr<MPSCQueue> queue{new MPSCQueue{size}};
        return std::make_pair(Sender{queue}, Receiver{queue});
    }

    MPSCQueue(MPSCQueue &other) = delete;

    MPSCQueue &operator=(const MPSCQueue &other) = delete;

    void add_sender() { sender.fetch_add(1); }

    void remove_sender() {
        if (sender.fetch_sub(1) == 0) {
            std::unique_lock<std::mutex> guard{lock};
            empty.notify_all();
        }
    }

    void remove_receiver() {
        if (receiver.fetch_sub(1) == 0) {
            std::unique_lock<std::mutex> guard{lock};
            full.notify_all();
        }
    }

    size_t sender_count() const { return sender.load(); }

    size_t receiver_count() const { return receiver.load(); }

    SenderSlotGuard try_send() {
        if (receiver.load() == 0) { return SenderSlotGuard{Error::Closed}; }

        sender_lock.lock();

        if (index_increase(end) == start.load()) {
            sender_lock.unlock();
            return SenderSlotGuard{Error::Empty};
        } else {
            return SenderSlotGuard{&inner[end.load()], this};
        }
    }

    SenderSlotGuard send() {
        if (receiver.load() == 0) { return SenderSlotGuard{Error::Closed}; }

        sender_lock.lock();

        if (index_increase(end.load()) == start.load()) {
            std::unique_lock<std::mutex> guard{lock};

            while (index_increase(end.load()) == start.load()) {
                full.wait(guard);

                if (receiver.load() == 0) {
                    sender_lock.unlock();
                    return SenderSlotGuard{Error::Closed};
                }
            }
        }

        return SenderSlotGuard{&inner[end.load()], this};
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
            if (sender.load() == 0) { return ReceiverSlotGuard{Error::Closed}; }
            else { return ReceiverSlotGuard{Error::Empty}; }
        } else {
            return ReceiverSlotGuard{&inner[start.load()], this};
        }
    }

    ReceiverSlotGuard recv() {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            if (sender.load() == 0) {
                receiver_lock.unlock();
                return ReceiverSlotGuard{Error::Closed};
            }

            std::unique_lock<std::mutex> guard{lock};

            while (start.load() == end.load()) {
                if (sender.load() == 0) {
                    receiver_lock.unlock();
                    return ReceiverSlotGuard{Error::Closed};
                }

                empty.wait(guard);
            }
        }

        return ReceiverSlotGuard{&inner[start.load()], this};
    }

    template<class RepT, class PeriodT>
    ReceiverSlotGuard recv_timeout(const std::chrono::duration<RepT, PeriodT> &period) {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            if (sender.load() == 0) {
                receiver_lock.unlock();
                return ReceiverSlotGuard{Error::Closed};
            }

            std::unique_lock<std::mutex> guard{lock};

            while (start.load() == end.load()) {
                if (sender.load() == 0) {
                    receiver_lock.unlock();
                    return ReceiverSlotGuard{Error::Closed};
                }

                if (empty.template wait_for(guard, period) == std::cv_status::timeout) {
                    receiver_lock.unlock();
                    return ReceiverSlotGuard{Error::Empty};
                }
            }
        }

        return ReceiverSlotGuard{&inner[start.load()], this};
    }

    template<typename ClockT, typename DurationT>
    ReceiverSlotGuard recv_deadline(const std::chrono::time_point<ClockT, DurationT> &time) {
        receiver_lock.lock();

        if (start.load() == end.load()) {
            if (sender.load() == 0) {
                receiver_lock.unlock();
                return ReceiverSlotGuard{Error::Closed};
            }

            std::unique_lock<std::mutex> guard{lock};

            while (start.load() == end.load()) {
                if (sender.load() == 0) {
                    receiver_lock.unlock();
                    return ReceiverSlotGuard{Error::Closed};
                }

                if (empty.template wait_until(guard, time) == std::cv_status::timeout) {
                    receiver_lock.unlock();
                    return ReceiverSlotGuard{Error::Empty};
                }
            }
        }

        return ReceiverSlotGuard{&inner[start.load()], this};
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
