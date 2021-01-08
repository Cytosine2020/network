#include <cstdarg>
#include <fcntl.h>
#include <memory>
#include <regex>

#include "application/ftp_server.hpp"


namespace cs120 {
FTPClient::FTPClient(std::shared_ptr<BaseSocket> &device, EndPoint local, EndPoint remote) :
        control{new TCPClient{device, 64, local, remote}}, data{nullptr} {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    if (!recv(control, buffer[Range{}])) { cs120_abort("connection failed!"); }
}

bool FTPClient::send_printf(std::unique_ptr<TCPClient> &client, MutSlice<uint8_t> buffer,
                            const char *format, ...) {
    if (client == nullptr) { return false; }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(reinterpret_cast<char *>(buffer.begin()), buffer.size(), format, args);
    va_end(args);

    if (len < 0 || static_cast<size_t>(len) >= buffer.size()) { return false; }

    auto msg = buffer[Range{0, static_cast<size_t>(len)}];

    if (msg.empty()) { return false; }

    for (size_t offset = 0, size = 0; offset < msg.size(); offset += size) {
        size = client->send(msg[Range{size}]);
        if (size == 0) { return false; }
    }

    return true;
}

bool FTPClient::recv(std::unique_ptr<TCPClient> &client, MutSlice<uint8_t> buffer) {
    if (client == nullptr) { return false; }

    size_t size = client->recv(buffer[Range{}]);
    if (size == 0) { return false; }

    printf("%.*s", static_cast<int>(size), buffer.begin());

    return true;
}

bool FTPClient::login(const char *user_name, const char *password) {
    return user(user_name) && pass(password);
}

bool FTPClient::user(const char *user_name) {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    return send_printf(control, buffer[Range{}], "USER %s\r\n", user_name) &&
           recv(control, buffer[Range{}]);
}

bool FTPClient::pass(const char *password) {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    return send_printf(control, buffer[Range{}], "PASS %s\r\n", password) &&
           recv(control, buffer[Range{}]);
}

bool FTPClient::pwd() {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    return send_printf(control, buffer[Range{}], "PWD\r\n") &&
           recv(control, buffer[Range{}]);
}

bool FTPClient::cwd(const char *pathname) {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    return send_printf(control, buffer[Range{}], "CWD %s\r\n", pathname) &&
           recv(control, buffer[Range{}]);
}

bool FTPClient::pasv(std::shared_ptr<cs120::BaseSocket> &device, EndPoint local) {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    if (!send_printf(control, buffer[Range{}], "PASV\r\n")) { return false; }

    size_t size = control->recv(buffer[Range{}]);
    if (size == 0) { return false; }

    buffer[size] = '\0';

    printf("%.*s", static_cast<int>(size), buffer.begin());

    std::regex reg{R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))"};
    std::cmatch match{};

    if (!std::regex_search(reinterpret_cast<char *>(buffer.begin()), match, reg)) {
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
    Buffer<uint8_t, BUFF_LEN> buffer{};

    if (!(send_printf(control, buffer[Range{}], "LIST %s\r\n", path) &&
          recv(control, buffer[Range{}]))) { return false; }

    while (!control->has_data()) {
        if (!recv(data, buffer[Range{}])) { return false; }
    }

    if (!recv(control, buffer[Range{}])) { return false; }

    return true;
}

bool FTPClient::quit() {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    return send_printf(control, buffer[Range{}], "QUIT\r\n") &&
           recv(control, buffer[Range{}]);
}

bool FTPClient::retr(const char *file_name) {
    Buffer<uint8_t, BUFF_LEN> buffer{};

    if (!(send_printf(control, buffer[Range{}], "RETR %s\r\n", file_name) &&
          recv(control, buffer[Range{}]))) { return false; }

    int file = open(file_name, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0644);
    if (file < 0) { cs120_abort("open error"); }

    while (!control->has_data()) {
        size_t size = data->recv(buffer[Range{}]);
        if (size == 0) { return false; }

        if (static_cast<size_t>(write(file, buffer.begin(), size)) != size) {
            cs120_abort("write error");
        }
    }

    close(file);

    if (!recv(control, buffer[Range{}])) { return false; }

    return true;
}
}
