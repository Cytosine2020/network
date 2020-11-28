#include "device/unix_socket.hpp"


namespace cs120 {
UnixSocket::UnixSocket(size_t buffer_size, size_t size) :
        receive_queue{nullptr}, send_queue{nullptr} {
    receive_queue = new SPSCQueue{buffer_size, size};
    send_queue = new SPSCQueue{buffer_size, size};


}

SPSCQueueSenderSlotGuard UnixSocket::send() { cs120_abort("unimplemented!"); }

SPSCQueueReceiverSlotGuard UnixSocket::recv() { cs120_abort("unimplemented!"); }
}
