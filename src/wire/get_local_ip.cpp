#include <wire/get_local_ip.hpp>


#include <utility.hpp>
#include <unistd.h>
#include <sys/wait.h>

#include "pcap/pcap.h"


namespace cs120 {
uint32_t get_local_ip() {
    const char PROMPT[] = "inet ";

    char pcap_error[PCAP_ERRBUF_SIZE]{};
    pcap_if_t *device = nullptr;
    if (pcap_findalldevs(&device, pcap_error) == PCAP_ERROR) { cs120_abort(pcap_error); }

    int pipe_fd[2] = {-1, -1};
    if (pipe(pipe_fd) != 0) { cs120_abort("pipe error"); }

    int child = fork();
    switch (child) {
        case -1:
            cs120_abort("fork error");
        case 0:
            dup2(pipe_fd[1], STDOUT_FILENO);
            if (execl("/sbin/ifconfig", "/sbin/ifconfig", device->name, nullptr) != 0) {
                cs120_abort("failed to execute ifconfig");
            }
        default:;
    }

    int child_state = 0;
    if (waitpid(child, &child_state, 0) != child) { cs120_abort("waitpid error"); }
    if (!WIFEXITED(child_state)) { cs120_abort("ifconfig execution failed!"); }

    char buffer[1024]{};
    ssize_t size = read(pipe_fd[0], buffer, 1024);
    if (size < 0) { cs120_abort("read error"); }

    uint32_t ip = inet_addr(strstr(buffer, PROMPT) + sizeof(PROMPT) - 1);
    if (ip == static_cast<uint32_t>(-1)) { cs120_abort("invalid ip"); }

    pcap_freealldevs(device);
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    return ip;
}
}
