#ifndef CS120_TCP_SERVER_HPP
#define CS120_TCP_SERVER_HPP


#include "pthread.h"
#include <unordered_map>
#include <list>

#include "device/base_socket.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
class TCPSender {
public:
    EndPoint addr;
    uint32_t frame_send, ack_receive, frame_receive;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end; // buffer end is frame received todo: memory order
    std::mutex lock, sender_lock;
    std::condition_variable full;
    bool closed;

    size_t index_increase(size_t index, size_t diff) const {
        return index + diff >= buffer.size() ? index + diff - buffer.size() : index + diff;
    }

    size_t get_window() const {
        if (buffer_end >= buffer_start) {
            return buffer.size() - 1 + buffer_start - buffer_end;
        } else {
            return buffer_start - buffer_end - 1;
        }
    }

    size_t get_size() const {
        if (buffer_end >= buffer_start) {
            return buffer_end - buffer_start;
        } else {
            return buffer.size() + buffer_end - buffer_start;
        }
    }

    TCPSender(EndPoint addr, uint32_t frame_send, uint32_t ack_receive, uint32_t frame_receive) :
    addr{addr}, frame_send{frame_send}, ack_receive{ack_receive}, frame_receive{frame_receive},
    buffer{4096}, buffer_start{}, buffer_end{}, closed{false} {}

    TCPSender(TCPSender &&other) noexcept = delete;

    TCPSender &operator=(TCPSender &&other) noexcept = delete;

    ssize_t send(Slice<uint8_t> data);

    void take(MutSlice<uint8_t> data);

    ~TCPSender() = default;
};


class TCPReceiver {
public:
    struct Fragment {
        uint32_t seq;
        uint32_t length;
    };

    EndPoint addr;
    uint32_t ack_receive, frame_receive;
    std::list<Fragment> fragments;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end; // buffer end is frame received todo: memory order
    std::mutex lock, receiver_lock;
    std::condition_variable empty;
    uint32_t close_seq;
    bool closing, closed;

    size_t index_increase(size_t index, size_t diff) const {
        return index + diff >= buffer.size() ? index + diff - buffer.size() : index + diff;
    }

    size_t get_window() const {
        if (buffer_end >= buffer_start) {
            return buffer.size() - 1 + buffer_start - buffer_end;
        } else {
            return buffer_start - buffer_end - 1;
        }
    }

    size_t get_size() const {
        if (buffer_end >= buffer_start) {
            return buffer_end - buffer_start;
        } else {
            return buffer.size() + buffer_end - buffer_start;
        }
    }

public:
    TCPReceiver(EndPoint addr, uint32_t ack_receive, uint32_t frame_receive) :
            addr{addr}, ack_receive{ack_receive}, frame_receive{frame_receive},
            fragments{}, buffer{4096}, buffer_start{0}, buffer_end{0},
            lock{}, receiver_lock{}, empty{}, close_seq{0}, closing{false}, closed{false} {}

    TCPReceiver(TCPReceiver &&other) noexcept = delete;

    TCPReceiver &operator=(TCPReceiver &&other) noexcept = delete;

    bool finished() { return closing && frame_receive == close_seq; }

    void accept(uint32_t seq, Slice<uint8_t> data);

    void close(uint32_t seq);

    ssize_t recv(MutSlice<uint8_t> data);

    ~TCPReceiver() = default;
};

class TCPConnection;

class TCPServer {
public:
    struct Request {
        enum {
            Create,
            FrameReceive,
            AckReceive,
            FrameSend,
            Close,
        } type;
        union {
            struct {
                TCPSender *sender;
            } create;
            struct {
                EndPoint dest;
                uint32_t ack_receive;
                uint32_t frame_receive;
            } frame_receive;
            struct {
                EndPoint dest;
                uint32_t ack_receive;
            } ack_receive;
            struct {
                EndPoint dest;
            } frame_send;
            struct {
                EndPoint dest;
                uint32_t ack_receive;
                uint32_t frame_receive;
            } close;
        } inner;
    };

private:
    struct TCPSenderArgs {
        size_t mtu;
        EndPoint addr;
        MPSCQueue<PacketBuffer>::Sender send_queue;
        MPSCQueue<Request>::Receiver request_receiver;
        std::unordered_map<EndPoint, TCPSender *> connection;
    };

    struct TCPReceiverArgs {
        EndPoint addr;
        Demultiplexer::ReceiverGuard recv_queue;
        MPSCQueue<PacketBuffer>::Sender send_queue;
        MPSCQueue<TCPServer::Request>::Sender request_sender;
        MPSCQueue<TCPConnection>::Sender connect_sender;
        std::unordered_map<EndPoint, TCPReceiver *> connection;
    };

    static void *tcp_sender(void *args);

    static void *tcp_receiver(void *args);

    pthread_t sender, receiver;
    std::shared_ptr<BaseSocket> device;
    MPSCQueue<TCPConnection>::Receiver connect_receiver;
    EndPoint addr;

public:
    TCPServer(std::shared_ptr<BaseSocket> &device, size_t size, EndPoint addr);

    TCPServer(TCPServer &&other) noexcept = default;

    TCPServer &operator=(TCPServer &&other) noexcept = default;

    TCPConnection accept();

    ~TCPServer() {
        pthread_join(sender, nullptr);
        pthread_join(receiver, nullptr);
    }
};


class TCPConnection {
private:
    EndPoint addr;
    // todo: free following two fields.
    TCPSender *sender;
    TCPReceiver *receiver;
    MPSCQueue<TCPServer::Request>::Sender request_sender;

public:
    TCPConnection() noexcept : addr{}, sender{nullptr}, receiver{nullptr} {}

    TCPConnection(EndPoint addr, TCPSender *sender, TCPReceiver *receiver,
                  MPSCQueue<TCPServer::Request>::Sender &request_sender) :
            addr{addr}, sender{sender}, receiver{receiver}, request_sender{request_sender} {}

    TCPConnection(TCPConnection &&other) noexcept = default;

    TCPConnection &operator=(TCPConnection &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data) {
        auto result = sender->send(data);

        *request_sender.send() = {TCPServer::Request::FrameSend, {.frame_send = {addr}}};

        return result;
    }

    ssize_t recv(MutSlice<uint8_t> data) { return receiver->recv(data); }

    ~TCPConnection() = default;
};


inline TCPConnection TCPServer::accept() { return std::move(*connect_receiver.recv()); }
}


#endif //CS120_TCP_SERVER_HPP
