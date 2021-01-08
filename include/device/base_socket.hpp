#ifndef CS120_BASE_SOCKET_HPP
#define CS120_BASE_SOCKET_HPP


#include "pthread.h"
#include <unordered_set>
#include <functional>
#include <memory>

#include "utility.hpp"
#include "queue.hpp"
#include "wire/ipv4.hpp"
#include "server/ipv4_server.hpp"


namespace cs120 {
using PacketBuffer = Buffer<uint8_t, 2048>;


template<typename T>
class Demultiplexer {
public:
    using Condition = std::function<bool(const IPV4Header *, Slice<uint8_t>, Slice<uint8_t>)>;

    struct Filter {
        Condition condition;
        typename MPSCQueue<T>::Sender queue;
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
        typename MPSCQueue<Request>::Sender sender;
        typename MPSCQueue<T>::Receiver receiver;

    public:
        ReceiverGuard() noexcept: filter{nullptr}, sender{}, receiver{} {}

        ReceiverGuard(
                std::shared_ptr<Filter> filter,
                typename MPSCQueue<Request>::Sender sender,
                typename MPSCQueue<T>::Receiver &&receiver
        ) : filter{std::move(filter)}, sender{std::move(sender)}, receiver{std::move(receiver)} {}

        ReceiverGuard(ReceiverGuard &&other) noexcept = default;

        ReceiverGuard &operator=(ReceiverGuard &&other) noexcept = default;

        typename MPSCQueue<T>::Receiver &operator*() { return receiver; }

        typename MPSCQueue<T>::Receiver *operator->() { return &receiver; }

        ~ReceiverGuard() {
            if (filter != nullptr) {
                auto buffer = sender.send();
                if (!buffer.none()) { *buffer = Request{Request::Remove, std::move(filter)}; }
            }
        };
    };

    class RequestSender {
    private:
        typename MPSCQueue<Request>::Sender sender;

    public:
        RequestSender() noexcept: sender{{nullptr}} {}

        explicit RequestSender(typename MPSCQueue<Request>::Sender sender) :
                sender{std::move(sender)} {}

        ReceiverGuard send(Condition &&condition, size_t size) {
            auto[send, recv] = MPSCQueue<T>::channel(size);

            auto filter = std::shared_ptr<Filter>{new Filter{
                    std::move(condition), std::move(send)
            }};

            { *sender.send() = Request{Request::Add, filter}; }

            return ReceiverGuard{std::move(filter), sender, std::move(recv)};
        }
    };

private:
    typename MPSCQueue<Request>::Sender sender;
    typename MPSCQueue<Request>::Receiver receiver;
    std::unordered_set<std::shared_ptr<Filter>> receivers;
    IPV4FragmentReceiver assembler;

public:
    explicit Demultiplexer(size_t size) : sender{nullptr}, receiver{nullptr}, receivers{} {
        auto[recv_sender, recv_receiver] = MPSCQueue<Request>::channel(size);

        sender = std::move(recv_sender);
        receiver = std::move(recv_receiver);
    }

    Demultiplexer(Demultiplexer &&other) noexcept = default;

    Demultiplexer &operator=(Demultiplexer &&other) noexcept = default;

    bool is_close() const { return sender.is_closed() && receivers.empty(); }

    void send(Slice<uint8_t> datagram) {
        for (;;) {
            auto request = receiver.try_recv();
            if (request.none()) { break; }

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

        auto buffer = assembler.recv(datagram);
        if (buffer.none()) { return; }

        auto[ip_header, ip_option, ip_data] = ipv4_split(*buffer);
        if (ip_header == nullptr) {
            cs120_warn("invalid package!");
            return;
        }

        for (auto &recv: receivers) {
            if (!recv->condition(ip_header, ip_option, ip_data)) { continue; }

            auto slot = recv->queue.try_send();
            if (slot.none()) {
                cs120_warn("package loss!");
            } else {
                auto ip_datagram = (*buffer)[Range{0, ip_header->get_total_length()}];
                (*slot)[Range{0, ip_datagram.size()}].copy_from_slice(ip_datagram);
            }
        }
    }

    RequestSender get_sender() { return RequestSender{sender}; }

    ~Demultiplexer() = default;
};


class BaseSocket {
public:
    virtual uint16_t get_mtu() = 0;

    virtual std::pair<MPSCQueue<PacketBuffer>::Sender, Demultiplexer<PacketBuffer>::ReceiverGuard>
    bind(Demultiplexer<PacketBuffer>::Condition &&condition, size_t size) = 0;

    virtual ~BaseSocket() = default;
};
}


#endif //CS120_BASE_SOCKET_HPP
