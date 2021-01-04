#ifndef CS120_TCP_SERVER_HPP
#define CS120_TCP_SERVER_HPP


#include "pthread.h"
#include <list>

#include "device/base_socket.hpp"
#include "wire/tcp.hpp"


namespace cs120 {
// todo
constexpr uint16_t WINDOW = 65535;
constexpr uint16_t IDENTIFIER = 0;

class TCPSender {
public:
    EndPoint local, remote;
    uint32_t frame_send, ack_receive, frame_receive;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end; // buffer end is frame received todo: memory order
    std::mutex lock, sender_lock;
    std::condition_variable full;
    uint32_t close_seq;
    bool closed;

    size_t index_increase(size_t index, size_t diff) const {
        return index + diff >= buffer.size() ? index + diff - buffer.size() : index + diff;
    }

    size_t get_size() const {
        if (buffer_end >= buffer_start) {
            return buffer_end - buffer_start;
        } else {
            return buffer.size() + buffer_end - buffer_start;
        }
    }

    TCPSender(EndPoint local, EndPoint remote, uint32_t local_seq, uint32_t remote_seq) :
            local{local}, remote{remote}, frame_send{local_seq}, ack_receive{local_seq},
            frame_receive{remote_seq}, buffer{4096}, buffer_start{}, buffer_end{}, closed{false} {}

    TCPSender(TCPSender &&other) noexcept = delete;

    TCPSender &operator=(TCPSender &&other) noexcept = delete;

    bool finished() const { return closed && frame_send == ack_receive; }

    ssize_t send(Slice<uint8_t> data);

    void take(uint32_t offset, MutSlice<uint8_t> data);

    void ack_update(uint32_t ack);

    void generate_ack(MPSCQueue<PacketBuffer>::Sender &sender) const {
        auto send = sender.try_send();
        if (send.none()) {
            cs120_warn("package lost!");
            return;
        }

        TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                            local.ip_addr, remote.ip_addr, local.port, remote.port,
                            frame_send, frame_receive,
                            false, false, false, false, true, false, false, false, false,
                            WINDOW, Slice<uint8_t>{}, 0);
    }

    void generate_data(MPSCQueue<PacketBuffer>::Sender &sender, uint32_t mss, uint32_t window) {
        uint32_t size;

        for (uint32_t offset = 0; offset < window; offset += size) {
            size = std::min(mss, window - offset);

            auto send = sender.try_send();
            if (send.none()) {
                frame_send = std::max(frame_send, ack_receive + offset);

                cs120_warn("package lost!");
                return;
            }

            auto tcp_buffer = TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                                  local.ip_addr, remote.ip_addr,
                                                  local.port, remote.port,
                                                  ack_receive + offset, frame_receive,
                                                  false, false, false, false, true,
                                                  offset + size == window, false, false, false,
                                                  WINDOW, Slice<uint8_t>{}, size);

            take(offset, *tcp_buffer);
        }

        generate_fin(sender);

        frame_send = std::max(frame_send, ack_receive + window);
    }

    void generate_fin(MPSCQueue<PacketBuffer>::Sender &sender) {
        if (closed && frame_send == close_seq) {
            auto send = sender.try_send();
            if (send.none()) {
                cs120_warn("package lost!");
                return;
            }

            TCPHeader::generate((*send)[Range{}], IDENTIFIER,
                                local.ip_addr, remote.ip_addr, local.port, remote.port,
                                frame_send++, frame_receive,
                                false, false, false, false, true, false, false, false, true,
                                WINDOW, Slice<uint8_t>{}, 0);
        }
    }

    ~TCPSender() = default;
};

class TCPReceiver {
public:
    struct Fragment {
        uint32_t seq;
        uint32_t length;
    };

    EndPoint local, remote;
    uint32_t ack_receive, frame_receive;
    std::list<Fragment> fragments;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end; // buffer end is frame received todo: memory order
    std::mutex lock, receiver_lock;
    std::condition_variable empty;
    uint32_t close_seq;
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

public:
    TCPReceiver(EndPoint local, EndPoint remote, uint32_t local_seq, uint32_t remote_seq) :
            local{local}, remote{remote}, ack_receive{local_seq}, frame_receive{remote_seq},
            fragments{}, buffer{4096}, buffer_start{0}, buffer_end{0},
            lock{}, receiver_lock{}, empty{}, close_seq{0}, closed{false} {}

    TCPReceiver(TCPReceiver &&other) noexcept = delete;

    TCPReceiver &operator=(TCPReceiver &&other) noexcept = delete;

    bool finished() const { return closed && frame_receive == close_seq + 1; }

    void accept(uint32_t seq, Slice<uint8_t> data);

    void close(uint32_t seq);

    ssize_t recv(MutSlice<uint8_t> data);

    ~TCPReceiver() = default;
};

class TCPClient {
public:
    struct Request {
        enum {
            FrameReceive,
            AckReceive,
            FrameSend,
            Close,
        } type;
        union {
            struct {
                uint32_t ack_receive;
                uint32_t frame_receive;
            } frame_receive;
            struct {
                uint32_t ack_receive;
            } ack_receive;
            struct {
            } frame_send;
            struct {
                uint32_t ack_receive;
                uint32_t frame_receive;
            } close;
        } inner;
    };

    struct SyncOption {
        TCPOptionMSS mss;

        explicit SyncOption(uint16_t mss) : mss{mss} {}

        Slice<uint8_t> into_slice() const {
            return Slice<uint8_t>{reinterpret_cast<const uint8_t *>(this), sizeof(SyncOption)};
        }

        MutSlice<uint8_t> into_slice() {
            return MutSlice<uint8_t>{reinterpret_cast<uint8_t *>(this), sizeof(SyncOption)};
        }
    };

private:
    struct TCPSendArgs {
        uint16_t mss;
        MPSCQueue<PacketBuffer>::Sender send_queue;
        MPSCQueue<Request>::Receiver request_receiver;
        TCPSender *connection;
    };

    struct TCPRecvArgs {
        Demultiplexer::ReceiverGuard recv_queue;
        MPSCQueue<TCPClient::Request>::Sender request_sender;
        TCPReceiver *connection;
    };

    struct TCPTimerArgs {
        MPSCQueue<TCPClient::Request>::Sender request_sender;
    };

    static void *tcp_sender(void *args);

    static void *tcp_receiver(void *args);

    pthread_t send_thread, recv_thread;
    std::shared_ptr<BaseSocket> device;
    TCPSender *sender;
    TCPReceiver *receiver;
    MPSCQueue<TCPClient::Request>::Sender request_sender;

public:
    TCPClient(std::shared_ptr<BaseSocket> &device, size_t size, EndPoint local, EndPoint remote);

    static std::pair<uint32_t, uint32_t> connect(EndPoint local, EndPoint remote, uint16_t mss,
                                                 MPSCQueue<PacketBuffer>::Sender &send_queue,
                                                 Demultiplexer::ReceiverGuard &recv_queue);

    TCPClient(TCPClient &&other) noexcept = default;

    TCPClient &operator=(TCPClient &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data) {
        auto result = sender->send(data);

        *request_sender.send() = TCPClient::Request{
                TCPClient::Request::FrameSend, {.frame_send = {}}
        };

        return result;
    }

    ssize_t recv(MutSlice<uint8_t> data) { return receiver->recv(data); }

    ~TCPClient() {
        {
            *request_sender.send() = Request{
                    Request::Close,
                    {.close = {receiver->ack_receive, receiver->frame_receive}}
            };
        }

        pthread_join(send_thread, nullptr);
        pthread_join(recv_thread, nullptr);
    }
};
}


#endif //CS120_TCP_SERVER_HPP
