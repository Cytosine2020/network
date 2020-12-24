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
    bool closed;

    TCPSender(EndPoint addr, uint32_t frame_send, uint32_t ack_receive, uint32_t frame_receive) :
    addr{addr}, frame_send{frame_send}, ack_receive{ack_receive}, frame_receive{frame_receive},
    closed{false} {}

    TCPSender(TCPSender &&other) noexcept = default;

    TCPSender &operator=(TCPSender &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data) {
        (void) data;

        // todo
        cs120_abort("unimplemented!");

        return 0;
    }

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

public:
    TCPReceiver(EndPoint addr, uint32_t ack_receive, uint32_t frame_receive) :
            addr{addr}, ack_receive{ack_receive}, frame_receive{frame_receive},
            fragments{}, buffer{4096}, buffer_start{0}, buffer_end{0},
            lock{}, receiver_lock{}, empty{}, closed{false} {}

    TCPReceiver(TCPReceiver &&other) noexcept = delete;

    TCPReceiver &operator=(TCPReceiver &&other) noexcept = delete;

    void accept(uint32_t seq, Slice<uint8_t> data);

    ssize_t recv(MutSlice<uint8_t> data);

    ~TCPReceiver() = default;
};


class TCPConnection {
private:
    EndPoint addr;
    // todo: free following two fields.
    TCPSender *sender;
    TCPReceiver *receiver;

public:
    TCPConnection() noexcept : addr{}, sender{nullptr}, receiver{nullptr} {}

    TCPConnection(EndPoint addr, TCPSender *sender, TCPReceiver *receiver) :
            addr{addr}, sender{sender}, receiver{receiver} {}

    TCPConnection(TCPConnection &&other) noexcept = default;

    TCPConnection &operator=(TCPConnection &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data) { return sender->send(data); }

    ssize_t recv(MutSlice<uint8_t> data) { return receiver->recv(data); }

    ~TCPConnection() = default;
};


class TCPServer {
private:
    struct Request {
        enum {
            Create,
            FrameReceive,
            AckReceive,
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
                uint32_t ack_receive;
                uint32_t frame_receive;
            } close;
        } inner;
    };

    struct TCPSenderArgs {
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
    std::unique_ptr<BaseSocket> device;
    MPSCQueue<TCPConnection>::Receiver connect_receiver;
    EndPoint addr;

public:
    TCPServer(std::unique_ptr<BaseSocket> device, size_t size, EndPoint addr);

    TCPServer(TCPServer &&other) noexcept = default;

    TCPServer &operator=(TCPServer &&other) noexcept = default;

    TCPConnection accept() { return std::move(*connect_receiver.recv()); }

    ~TCPServer() = default;
};
}


#endif //CS120_TCP_SERVER_HPP
