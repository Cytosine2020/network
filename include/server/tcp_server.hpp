#ifndef CS120_TCP_SERVER_HPP
#define CS120_TCP_SERVER_HPP


#include "pthread.h"
#include <list>

#include "device/base_socket.hpp"
#include "wire/tcp.hpp"
#include "ipv4_server.hpp"


namespace cs120 {
// todo
constexpr uint16_t IDENTIFIER = 0;

class TCPSender {
public:
    static constexpr size_t BUFFER_SIZE = 1 << 16;

    EndPoint local, remote;
    uint16_t mss;
    uint8_t scale;
    uint32_t frame_send, ack_receive, frame_receive;
    uint32_t local_window, remote_window;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end;
    std::mutex lock, sender_lock;
    std::condition_variable full;
    uint32_t close_seq;
    bool closed;

    size_t index_increase(size_t index, size_t diff) const {
        return index + diff >= buffer.size() ? index + diff - buffer.size() : index + diff;
    }

    uint32_t get_buffer_size() const {
        if (buffer_end >= buffer_start) {
            return buffer_end - buffer_start;
        } else {
            return buffer.size() + buffer_end - buffer_start;
        }
    }

    uint32_t get_size() const {
        if (remote_window == 0) { return 1; }
        return std::min(get_buffer_size(), remote_window);
    }

    TCPSender(EndPoint local, EndPoint remote, uint32_t local_seq, uint32_t remote_seq,
              uint16_t mss, uint8_t scale, uint32_t remote_window) :
            local{local}, remote{remote}, mss{mss}, scale{scale},
            frame_send{local_seq}, ack_receive{local_seq}, frame_receive{remote_seq},
            local_window{BUFFER_SIZE - 1}, remote_window{remote_window},
            buffer{BUFFER_SIZE}, buffer_start{}, buffer_end{}, close_seq{0}, closed{false} {}

    TCPSender(TCPSender &&other) noexcept = delete;

    TCPSender &operator=(TCPSender &&other) noexcept = delete;

    bool finished() const {
        return closed && frame_send == ack_receive && frame_send == close_seq + 1;
    }

    ssize_t send(Slice<uint8_t> data);

    void take(uint32_t offset, MutSlice<uint8_t> data);

    void ack_update(uint32_t ack);

    uint16_t get_tcp_window() const {
        uint32_t window = local_window >> scale;
        uint32_t max = std::numeric_limits<uint16_t>::max();
        return std::min(window, max);
    }

    void generate_ack(MPSCQueue<PacketBuffer>::Sender &sender) const {
        auto send = sender.try_send().unwrap();
        if (send.none()) {
            cs120_warn("package lost!");
            return;
        }

        TCPHeader::generate((*send)[Range{}], 0, IDENTIFIER,
                            local.ip_addr, remote.ip_addr, 64, local.port, remote.port,
                            frame_send, frame_receive,
                            false, false, false, false, true, false, false, false, false,
                            get_tcp_window(), Slice<uint8_t>{}, 0);
    }

    void generate_data(MPSCQueue<PacketBuffer>::Sender &sender) {
        uint32_t window = get_size();
        uint32_t size;

        for (uint32_t offset = 0; offset < window; offset += size) {
            size = std::min<size_t>(mss, window - offset);

            auto send = sender.try_send().unwrap();
            if (send.none()) {
                frame_send = std::max(frame_send, ack_receive + offset);

                cs120_warn("package lost!");
                return;
            }

            auto tcp_buffer = TCPHeader::generate((*send)[Range{}], 0, IDENTIFIER,
                                                  local.ip_addr, remote.ip_addr, 64,
                                                  local.port, remote.port,
                                                  ack_receive + offset, frame_receive,
                                                  false, false, false, false, true,
                                                  offset + size == window, false, false, false,
                                                  get_tcp_window(), Slice<uint8_t>{}, size);

            take(offset, *tcp_buffer);
        }

        generate_fin(sender);

        frame_send = std::max(frame_send, ack_receive + window);
    }

    void generate_fin(MPSCQueue<PacketBuffer>::Sender &sender) {
        if (closed) {
            auto send = sender.try_send().unwrap();
            if (send.none()) {
                cs120_warn("package lost!");
                return;
            }

            TCPHeader::generate((*send)[Range{}], 0, IDENTIFIER,
                                local.ip_addr, remote.ip_addr, 64, local.port, remote.port,
                                close_seq, frame_receive,
                                false, false, false, false, true, false, false, false, true,
                                get_tcp_window(), Slice<uint8_t>{}, 0);

            frame_send = close_seq + 1;
        }
    }

    ~TCPSender() = default;
};

class TCPReceiver {
public:
    static constexpr size_t BUFFER_SIZE = 1 << 16;

    EndPoint local, remote;
    uint16_t mss;
    uint8_t scale;
    uint32_t ack_receive, frame_receive;
    std::map<uint32_t, uint32_t> fragments;
    Array<uint8_t> buffer;
    size_t buffer_start, buffer_end;
    std::mutex lock, receiver_lock;
    std::condition_variable empty;
    uint32_t close_seq;
    bool closed;

    size_t index_increase(size_t index, size_t diff) const {
        return index + diff >= buffer.size() ? index + diff - buffer.size() : index + diff;
    }

    size_t get_window() const {
        if (buffer_end >= buffer_start) {
            return buffer.size() + buffer_start - buffer_end - 1;
        } else {
            return buffer_start - buffer_end - 1;
        }
    }

public:
    TCPReceiver(EndPoint local, EndPoint remote, uint32_t local_seq, uint32_t remote_seq,
                uint16_t mss, uint8_t scale) :
            local{local}, remote{remote}, mss{mss}, scale{scale},
            ack_receive{local_seq}, frame_receive{remote_seq},
            fragments{}, buffer{BUFFER_SIZE}, buffer_start{0}, buffer_end{0},
            lock{}, receiver_lock{}, empty{}, close_seq{0}, closed{false} {}

    TCPReceiver(TCPReceiver &&other) noexcept = delete;

    TCPReceiver &operator=(TCPReceiver &&other) noexcept = delete;

    bool finished() const { return closed && frame_receive == close_seq + 1; }

    void accept(uint32_t seq, Slice<uint8_t> data);

    void close(uint32_t seq);

    ssize_t recv(MutSlice<uint8_t> data);

    bool has_data() { return buffer_start != buffer_end; }

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
                uint32_t remote_window;
                uint32_t frame_receive;
                uint32_t local_window;
            } frame_receive;
            struct {
                uint32_t ack_receive;
                uint32_t remote_window;
            } ack_receive;
            struct {
            } frame_send;
            struct {
                uint32_t ack_receive;
                uint32_t frame_receive;
            } close;
        } inner;
    };

    struct SyncOption : public IntoSliceTrait<SyncOption> {
        TCPOptionMSS mss;
        TCPOptionScale scale;

        explicit SyncOption(uint16_t mss, uint8_t scale) : mss{mss}, scale{scale} {}
    };

private:
    struct TCPSendArgs {
        MPSCQueue<PacketBuffer>::Sender send_queue;
        MPSCQueue<Request>::Receiver request_receiver;
        std::shared_ptr<TCPSender> connection;
    };

    struct TCPRecvArgs {
        Demultiplexer<PacketBuffer>::ReceiverGuard recv_queue;
        MPSCQueue<TCPClient::Request>::Sender request_sender;
        std::shared_ptr<TCPReceiver> connection;
    };

    static void *tcp_sender(void *args);

    static void *tcp_receiver(void *args);

    pthread_t send_thread, recv_thread;
    std::shared_ptr<BaseSocket> device;
    std::shared_ptr<TCPSender> sender;
    std::shared_ptr<TCPReceiver> receiver;
    MPSCQueue<TCPClient::Request>::Sender request_sender;

public:
    TCPClient() :
            send_thread{}, recv_thread{}, device{},
            sender{nullptr}, receiver{nullptr}, request_sender{} {};

    TCPClient(std::shared_ptr<BaseSocket> &device, size_t size, EndPoint local, EndPoint remote);

    TCPClient(TCPClient &&other) noexcept = default;

    TCPClient &operator=(TCPClient &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data) {
        auto result = sender->send(data);

        auto send = request_sender.send();
        if (!send.none()) {
            *send = TCPClient::Request{TCPClient::Request::FrameSend, {.frame_send = {}}};
        }

        return result;
    }

    ssize_t recv(MutSlice<uint8_t> data) { return receiver->recv(data); }

    bool has_data() { return receiver->has_data(); }

    ~TCPClient() {
        {
            auto send = request_sender.try_send();
            if (!send.none()) {
                *send = Request{Request::Close, {.close = {
                        receiver->ack_receive, receiver->frame_receive
                }}};
            }
        }

//        pthread_join(send_thread, nullptr);
//        pthread_join(recv_thread, nullptr);
    }
};
}


#endif //CS120_TCP_SERVER_HPP
