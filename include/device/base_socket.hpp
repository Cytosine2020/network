#ifndef CS120_BASE_SOCKET_HPP
#define CS120_BASE_SOCKET_HPP


#include "pthread.h"
#include <unordered_set>
#include <functional>
#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "wire/ipv4.hpp"


namespace cs120 {
using PacketBuffer = Buffer<uint8_t, 2048>;


class Demultiplexer {
public:
    using Condition = std::function<bool(const IPV4Header *, Slice<uint8_t>, Slice<uint8_t>)>;

    struct Filter {
        Condition condition;
        MPSCQueue<PacketBuffer>::Sender queue;
    };

    struct Request {
        enum {
            Add,
            Remove,
        } type;
        std::shared_ptr<Filter> filter;
    };

    class ReceiverGuard {
    private:
        std::shared_ptr<Filter> filter;
        MPSCQueue<Request>::Sender sender;
        MPSCQueue<PacketBuffer>::Receiver receiver;

    public:
        ReceiverGuard() : filter{nullptr}, sender{}, receiver{} {}

        ReceiverGuard(
                std::shared_ptr<Filter> filter,
                MPSCQueue<Request>::Sender sender,
                MPSCQueue<PacketBuffer>::Receiver &&receiver
        ) : filter{std::move(filter)}, sender{std::move(sender)}, receiver{std::move(receiver)} {}

        ReceiverGuard(ReceiverGuard &&other) noexcept = default;

        ReceiverGuard &operator=(ReceiverGuard &&other) noexcept = default;

        MPSCQueue<PacketBuffer>::Receiver &operator*() { return receiver; }

        MPSCQueue<PacketBuffer>::Receiver *operator->() { return &receiver; }

        ~ReceiverGuard() {
            if (filter != nullptr){
                *sender.send() = Request{Request::Remove, std::move(filter)};
            }
        };
    };

    class RequestSender {
    private:
        MPSCQueue<Request>::Sender sender;

    public:
        RequestSender() : sender{{nullptr}} {}

        explicit RequestSender(MPSCQueue<Request>::Sender sender) : sender{std::move(sender)} {}

        ReceiverGuard send(Condition &&condition, size_t size) {
            auto[send, recv] = MPSCQueue<PacketBuffer>::channel(size);

            auto filter = std::shared_ptr<Filter>{new Filter{
                std::move(condition), std::move(send)
            }};

            { *sender.send() = Request{Request::Add, filter}; }

#if defined(__APPLE__)
            pthread_yield_np();
#elif defined(__linux__)
            pthread_yield();
#endif

            return ReceiverGuard{std::move(filter), sender, std::move(recv)};
        }
    };

private:
    MPSCQueue<Request>::Sender sender;
    MPSCQueue<Request>::Receiver receiver;
    std::unordered_set<std::shared_ptr<Filter>> receivers;

public:
    explicit Demultiplexer(size_t size) : sender{nullptr}, receiver{nullptr}, receivers{} {
        auto[recv_sender, recv_receiver] = MPSCQueue<Request>::channel(size);

        sender = std::move(recv_sender);
        receiver = std::move(recv_receiver);
    }

    Demultiplexer(Demultiplexer &&other) noexcept = default;

    Demultiplexer &operator=(Demultiplexer &&other) noexcept = default;

    void update() {
        for (;;) {
            auto request = receiver.try_recv();
            if (request.empty()) { break; }

            switch (request->type) {
                case Request::Add:
                    receivers.emplace(std::move(request->filter));
                    break;
                case Request::Remove:
                    receivers.erase(request->filter);
                    break;
                default:
                    cs120_unreachable("unknown request!");
            }
        }
    }

    void send(Slice<uint8_t> datagram) {
        auto[ip_header, ip_option, ip_data] = ipv4_split(datagram);
        if (ip_header == nullptr) {
            cs120_warn("invalid package!");
            return;
        }

        for (auto &recv: receivers) {
            if (!recv->condition(ip_header, ip_option, ip_data)) { continue; }

            auto slot = recv->queue.try_send();
            if (slot->empty()) {
                cs120_warn("package loss!");
            } else {
                auto ip_datagram = datagram[Range{0, ip_header->get_total_length()}];
                (*slot)[Range{0, ip_datagram.size()}].copy_from_slice(ip_datagram);
            }
        }
    }

    RequestSender get_sender() { return RequestSender{sender}; }

    ~Demultiplexer() = default;
};


class BaseSocket {
public:
    virtual size_t get_mtu() = 0;

    virtual std::pair<MPSCQueue<PacketBuffer>::Sender, Demultiplexer::ReceiverGuard>
    bind(Demultiplexer::Condition &&condition, size_t size) = 0;

    virtual ~BaseSocket() = default;
};
}


#endif //CS120_BASE_SOCKET_HPP
