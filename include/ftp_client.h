#ifndef FTP_FTP_CLIENT_H
#define FTP_FTP_CLIENT_H

#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex>
#include <memory>
#include "wire/tcp.hpp"
#include "device/raw_socket.hpp"
#include "wire/wire.hpp"
#include "wire/get_local_ip.hpp"
#include "server/tcp_server.hpp"


using namespace cs120;

constexpr size_t BUFF_LEN = 1 << 16;

class FTPClient {
private:
    std::unique_ptr<TCPClient> control;
    std::unique_ptr<TCPClient> data;

    static bool send_printf(std::unique_ptr<TCPClient> &client, MutSlice<uint8_t> buffer,
                            const char *format, ...);

    static bool recv(std::unique_ptr<TCPClient> &client, MutSlice<uint8_t> buffer);

public:
    FTPClient(std::shared_ptr<BaseSocket> &device, EndPoint local, EndPoint remote);

    bool login(const char *user_name, const char *password);

    bool pasv(std::shared_ptr<cs120::BaseSocket> &device, EndPoint local);

    bool user(const char *user_name);

    bool pass(const char *password);

    bool pwd();

    bool cwd(const char *pathname);

    bool list(const char *path);

    bool retr(const char *file_name);

    bool quit();
};


#endif //FTP_FTP_CLIENT_H
