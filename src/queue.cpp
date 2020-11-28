#include "queue.hpp"


namespace cs120 {
bool SPSCQueue::send(Slice<uint8_t> item) {
    std::unique_lock sender_guard{sender_lock};

    if (index_increase(end) == start) { return false; }

    get_slot(end)[Range{0, item.size()}].shallow_copy_from_slice(item[Range{}]);

    std::unique_lock guard{lock};

    end = index_increase(end);

    variable.notify_one();

    return true;
}

void SPSCQueue::commit() {

}


Array<uint8_t> SPSCQueue::recv() {
    std::unique_lock receiver_guard{receiver_lock};

    if (start == end) {
        std::unique_lock guard{lock};

        while (start == end) {
            variable.wait(guard);
        }
    }

    Array <uint8_t> item{buffer_size};

    item.shallow_copy_from_slice(get_slot(start));

    std::unique_lock guard{lock};

    start = index_increase(start);

    return item;
}

void SPSCQueue::claim() {

}
}
