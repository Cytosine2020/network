#ifndef CS120_TCP_SERVER_HPP
#define CS120_TCP_SERVER_HPP


#include <pthread.h>

#include "device/base_socket.hpp"


namespace cs120 {
enum class TCPState {
    Listen,
    SyncSent,
    SyncReceived,
    Established,
    FinWait1,
    FinWait2,
    CloseWait,
    Closing,
    LastAck,
    TimeWait,
    Closed,
};


class TCPListener {
private:

public:

};


class TCPServer {
private:
    std::unique_ptr<BaseSocket> device;
    MPSCQueue<PacketBuffer>::Sender send_queue;
    Demultiplexer::ReceiverGuard recv_queue;
    uint32_t src_ip, dest_ip;
    uint16_t src_port, dest_port;
    uint16_t identifier;
    TCPState status;
    uint16_t window;
    uint32_t src_seq, dest_seq;

    TCPServer(std::unique_ptr<BaseSocket> device, size_t size,
              uint32_t src_ip, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port,
              TCPState status);

    void accept();

public:
    static TCPServer accept(std::unique_ptr<BaseSocket> device, size_t size,
                            uint32_t src_ip, uint32_t dest_ip,
                            uint16_t src_port, uint16_t dest_port);

    TCPServer(TCPServer &&other) noexcept = default;

    TCPServer &operator=(TCPServer &&other) noexcept = default;

    ssize_t send(Slice<uint8_t> data);

    ssize_t recv(MutSlice<uint8_t> data);

    void close();

    ~TCPServer() { close(); }
};
}


#endif //CS120_TCP_SERVER_HPP
