//
// Created by zhao on 2021/1/3.
//

#ifndef FTP_FTP_CLIENT_H
#define FTP_FTP_CLIENT_H
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <regex>
#include "wire/tcp.hpp"
#include "device/raw_socket.hpp"
#include "wire/wire.hpp"
#include "wire/get_local_ip.hpp"
#include "server/tcp_server.hpp"
#define BUFF_LEN 10*1024
using namespace cs120;
class ftp_client {

    uint16_t data_port;
    TCPClient *connect;
    TCPClient *data_connect;
    std::shared_ptr<cs120::BaseSocket> lan;
    std::shared_ptr<cs120::BaseSocket> data_lan;
public:
    ftp_client(char* servername,std::shared_ptr<cs120::BaseSocket> sock1,std::shared_ptr<cs120::BaseSocket> sock2);

    int login(char* user_name, char* password);

    int PASV();

    int PWD();

    int CWD(char* pathname);

    int LIST(char * listname);

    int RETR(char* filename);

};


#endif //FTP_FTP_CLIENT_H
