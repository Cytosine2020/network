#include <cstdarg>
#include <fcntl.h>
#include <memory>
#include <regex>

#include "application/ftp_server.hpp"


namespace {
using namespace cs120;

void print_slice(Slice<uint8_t> data) {
    printf("%.*s", static_cast<int>(data.size()), data.begin());
}
}

namespace cs120 {
FTPClient::FTPClient(std::shared_ptr<BaseSocket> &device, EndPoint local, EndPoint remote) :
        control{new TCPClient{device, 64, local, remote}}, data{nullptr},
        write_buffer{BUFF_LEN}, read_buffer{BUFF_LEN}, read_remain{} {
    auto msg = recv_line();
    if (msg.empty()) { cs120_abort("connection failed!"); }
    print_slice(msg);
}

bool FTPClient::send_printf(const char *format, ...) {
    if (control == nullptr) { return false; }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(reinterpret_cast<char *>(write_buffer.begin()),
                        write_buffer.size(), format, args);
    va_end(args);

    if (len < 0 || static_cast<size_t>(len) >= write_buffer.size()) { return false; }

    auto msg = write_buffer[Range{0, static_cast<size_t>(len)}];

    if (msg.empty()) { return false; }

    for (size_t offset = 0, size = 0; offset < msg.size(); offset += size) {
        size = control->send(msg[Range{size}]);
        if (size == 0) { return false; }
    }

    return true;
}

Slice<uint8_t> FTPClient::recv_line() {
    if (control == nullptr) { return MutSlice<uint8_t>{}; }

    for (size_t i = read_remain.size(); i > 2; --i) {
        if (read_remain[i - 2] == '\r' && read_remain[i - 1] == '\n') {
            read_remain = read_remain[Range{i}];
            return read_remain[Range{0, i}];
        }
    }

    size_t offset = read_remain.size();

    read_buffer[Range{0, offset}].copy_from_slice(read_remain);

    read_remain = MutSlice<uint8_t>{};

    for (size_t size; offset < read_buffer.size(); offset += size) {
        size = control->recv(read_buffer[Range{offset}]);
        if (size == 0) { return MutSlice<uint8_t>{}; }

        auto read = read_buffer[Range{offset}][Range{0, size}];

        for (size_t i = size; i > 2; --i) {
            if (read[i - 2] == '\r' && read[i - 1] == '\n') {
                read_remain = read[Range{i}];
                return read_buffer[Range{0, offset + i}];
            }
        }
    }

    return MutSlice<uint8_t>{};
}

bool FTPClient::user(const char *user_name) {
     if (!send_printf("USER %s\r\n", user_name)) { return false; }

     auto msg = recv_line();
     if (msg.empty()) { return false; }
    print_slice(msg);

     return true;
}

bool FTPClient::pass(const char *password) {
    if (!send_printf("PASS %s\r\n", password)) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}

bool FTPClient::pwd() {
    if (!send_printf("PWD\r\n")) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}

bool FTPClient::cwd(const char *pathname) {
    if (!send_printf("CWD %s\r\n", pathname)) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}

bool FTPClient::pasv(std::shared_ptr<cs120::BaseSocket> &device, EndPoint local) {
    if (!send_printf("PASV\r\n")) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    std::regex reg{R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))"};
    std::cmatch match{};
    if (!std::regex_search(reinterpret_cast<const char *>(msg.begin()), match, reg)) {
        cs120_abort("");
    }

    uint32_t remote_ip = (stoi(match.str(4)) << 24) + (stoi(match.str(3)) << 16) +
                         (stoi(match.str(2)) << 8) + stoi(match.str(1));
    uint16_t remote_port = (stoi(match.str(5)) << 8) + stoi(match.str(6));

    EndPoint remote{remote_ip, remote_port};

    data = std::make_unique<TCPClient>(device, 64, local, remote);

    return true;
}

bool FTPClient::list(const char *path) {
    if (data == nullptr) { return false; }

    if (!send_printf("LIST %s\r\n", path)) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    Buffer<uint8_t, BUFF_LEN> buffer{};
    for (;;) {
        size_t size = data->recv(buffer[Range{}]);
        if (size == 0) { break; }

        print_slice(buffer[Range{0, size}]);
    }

    data = nullptr;

    msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}

bool FTPClient::retr(const char *file_name) {
    if (data == nullptr) { return false; }

    if (!send_printf("RETR %s\r\n", file_name)) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    int file = open(file_name, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (file < 0) { cs120_abort("open error"); }

    Buffer<uint8_t, BUFF_LEN> buffer{};
    for (;;) {
        size_t size = data->recv(buffer[Range{}]);
        if (size == 0) { break; }

        if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
            cs120_abort("write error");
        }
    }

    data = nullptr;
    close(file);

    msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}

bool FTPClient::quit() {
    if (!send_printf("QUIT\r\n")) { return false; }

    auto msg = recv_line();
    if (msg.empty()) { return false; }
    print_slice(msg);

    return true;
}
}
