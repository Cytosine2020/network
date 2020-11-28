#include "device/unix_socket.hpp"


namespace cs120 {

SPSCQueueSenderSlotGuard UnixSocket::send() { cs120_abort("unimplemented!"); }

SPSCQueueReceiverSlotGuard UnixSocket::recv() { cs120_abort("unimplemented!"); }
}
