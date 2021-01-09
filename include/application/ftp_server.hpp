#ifndef FTP_FTP_CLIENT_H
#define FTP_FTP_CLIENT_H


#include <unistd.h>
#include <memory>

#include "wire/tcp.hpp"
#include "device/raw_socket.hpp"
#include "server/tcp_server.hpp"


namespace cs120 {
class FTPClient {
private:
    static constexpr size_t BUFF_LEN = 1 << 16;

    std::unique_ptr<TCPClient> control;
    std::unique_ptr<TCPClient> data;
    Array<uint8_t> write_buffer, read_buffer;
    Slice<uint8_t> read_remain;

    bool send_printf(const char *format, ...);

    Slice<uint8_t> recv_line();

public:
    FTPClient(std::shared_ptr<BaseSocket> &device, EndPoint local, EndPoint remote);

    bool login(const char *user_name, const char *password);

    bool pasv(std::shared_ptr<cs120::BaseSocket> &device, EndPoint local);

    bool user(const char *user_name);

    bool pass(const char *password);

    bool pwd();

    bool cwd(const char *pathname);

    bool list(const char *path = "");

    bool retr(const char *file_name);

    bool quit();

    ~FTPClient() { quit(); }
};
}


#endif //FTP_FTP_CLIENT_H
