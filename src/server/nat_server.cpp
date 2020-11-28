#include "server/nat_server.hpp"


namespace cs120 {
void *NatServer::nat_lan_to_wan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    for (;;) {
        auto receive = args->lan->recv();

        // todo: add new mapping to nat table

        auto send = args->wan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            (*send)[Range{0, receive->size()}].copy_from_slice(*receive);
        }
    }

    return nullptr;
}

void *NatServer::nat_wan_to_lan(void *args_) {
    auto *args = reinterpret_cast<NatServer *>(args_);

    for (;;) {
        auto receive = args->wan->recv();

        // todo: check nat table

        auto send = args->lan->try_send();
        if (send->empty()) {
            cs120_warn("package loss!");
        } else {
            (*send)[Range{0, receive->size()}].copy_from_slice(*receive);
        }
    }

    return nullptr;
}

NatServer::NatServer(std::unique_ptr<BaseSocket> lan, std::unique_ptr<BaseSocket> wan) :
        lan_to_wan{}, wan_to_lan{},
        lan{std::move(lan)}, wan{std::move(wan)},
        lock{}, nat_table{1024} {

    pthread_create(&lan_to_wan, nullptr, nat_lan_to_wan, this);
    pthread_create(&wan_to_lan, nullptr, nat_wan_to_lan, this);
}
}
