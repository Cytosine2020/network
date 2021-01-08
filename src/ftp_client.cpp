#include <cstdarg>
#include <cstring>
#include <fcntl.h>

#include "ftp_client.h"

using namespace cs120;


uint32_t get_host_ip(const char *host) {
    struct hostent *server_ip = gethostbyname(host);
    return reinterpret_cast<struct in_addr *>(server_ip->h_addr_list[0])->s_addr;
}

MutSlice<uint8_t> buffer_printf(MutSlice<uint8_t> buffer, const char *format, va_list args) {
    int size = vsnprintf(reinterpret_cast<char *>(buffer.begin()), buffer.size(), format, args);

    if (size < 0 || static_cast<size_t>(size) >= buffer.size()) { return MutSlice < uint8_t > {}; }

    return buffer[Range{0, static_cast<size_t>(size)}];
}

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
    auto msg = buffer_printf(buffer[Range{}], format, args);
    va_end(args);

    if (msg.empty()) { return false; }

    for (size_t offset = 0, size = 0; offset < msg.size(); offset += size) {
        size = client->send(msg[Range{size}]);
        if (size == 0) { return false; }
    }

    return true;
}

bool FTPClient::recv(std::unique_ptr<TCPClient> &client, MutSlice<uint8_t> buffer) {
    if (client == nullptr) { return false; }

//    while (client->has_data()) {
    size_t size = client->recv(buffer[Range{}]);
    if (size == 0) { return false; }

    printf("%.*s", static_cast<int>(size), buffer.begin());
//    }

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
    if (!(send_printf(control, buffer[Range{}], "PASV\r\n") &&
          recv(control, buffer[Range{}]))) { return false; }

    std::regex reg(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    std::cmatch match{};

    uint16_t remote_port;
    uint32_t remote_ip;
    if (std::regex_search(reinterpret_cast<char *>(buffer.begin()), match, reg)) {
        remote_port = (stoi(match.str(5)) << 8) + stoi(match.str(6));
        remote_ip = (stoi(match.str(4)) << 24) + (stoi(match.str(3)) << 16) +
                    (stoi(match.str(2)) << 8) + stoi(match.str(1));
    } else {
        cs120_abort("");
    }

    EndPoint remote{remote_ip, remote_port};

    printf("crate new tcp connection %s:%hu\n", inet_ntoa(in_addr{remote.ip_addr}), remote.port);

    data = std::unique_ptr<TCPClient>{new TCPClient(device, 64, local, remote)};

    return true;
}

bool FTPClient::list(const char *path = nullptr) {
    Buffer<uint8_t, BUFF_LEN> buffer{};
    if (path == nullptr) {
        if (!send_printf(control, buffer[Range{}], "LIST\r\n")) { return false; }
    } else {
        if (!send_printf(control, buffer[Range{}], "LIST %s\r\n", path)) { return false; }
    }

    if (!recv(control, buffer[Range{}])) { return false; }

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

int main(int argc, char **argv) {
    if (argc != 3) { cs120_abort("accept 2 arguments"); }

    std::shared_ptr<BaseSocket> device{new RawSocket{64}};

    auto[local_ip, local_port] = parse_ip_address(argv[1]);

    EndPoint remote{get_host_ip(argv[2]), 21};
    EndPoint local{local_ip, local_port++};

    FTPClient ftp_client(device, local, remote);

    ftp_client.login("ftp", "");
    ftp_client.pwd();
    ftp_client.pasv(device, EndPoint{local_ip, local_port++}) &&
    ftp_client.list();
    ftp_client.cwd("ubuntu");
    ftp_client.pasv(device, EndPoint{local_ip, local_port++}) &&
    ftp_client.retr("ls-lR.gz");
    ftp_client.quit();
}
