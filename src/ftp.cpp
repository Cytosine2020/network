#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>

#include "utility.hpp"
#include "wire/wire.hpp"
#include "device/raw_socket.hpp"
#include "device/unix_socket.hpp"
#include "device/athernet_socket.hpp"
#include "application/ftp_server.hpp"


using namespace cs120;


uint32_t get_host_ip(const char *host) {
    struct hostent *server_ip = gethostbyname(host);
    return reinterpret_cast<struct in_addr *>(server_ip->h_addr_list[0])->s_addr;
}


int main(int argc, char **argv) {
    if (argc < 5) { cs120_abort("accept 4 arguments"); }

    auto device = argv[1];
    auto[local_ip, local_port] = parse_ip_address(argv[2]);
    auto user = argv[3];
    auto host = argv[4];

    std::shared_ptr<BaseSocket> sock{};
    if (strcmp(device, "-a") == 0) {
        sock = std::shared_ptr<BaseSocket>{new UnixSocket{64}};
    } else if (strcmp(device, "-e") == 0) {
        sock = std::shared_ptr<BaseSocket>{new AthernetSocket{64}};
    } else if (strcmp(device, "-r") == 0) {
        sock = std::shared_ptr<BaseSocket>{new RawSocket{64}};
    } else {
        cs120_abort("unknown command");
    }

    EndPoint remote{get_host_ip(host), 21};
    EndPoint local{local_ip, local_port++};

    FTPClient ftp_client{sock, local, remote};

    ftp_client.user(user);

    if (argc > 5) {
        ftp_client.pass(argv[5]);
    } else {
        ftp_client.pass("");
    }

    for (;;) {
        std::string str{};

        printf("%s@%s> ", user, host);
        getline(std::cin, str);

        if (str == "pwd") {
            ftp_client.pwd();
        } else if (str.rfind("cd ", 0) == 0) {
            ftp_client.cwd(str.substr(3).c_str());
        } else if (str.rfind("ls ", 0) == 0) {
            if (!ftp_client.data_link()) {
                ftp_client.pasv(sock, EndPoint{local_ip, local_port++});
            }
            ftp_client.list(str.substr(3).c_str());
        } else if (str == "ls") {
            if (!ftp_client.data_link()) {
                ftp_client.pasv(sock, EndPoint{local_ip, local_port++});
            }
            ftp_client.list();
        } else if (str.rfind("retr ", 0) == 0) {
            if (!ftp_client.data_link()) {
                ftp_client.pasv(sock, EndPoint{local_ip, local_port++});
            }
            ftp_client.retr(str.substr(5).c_str());
        } else if (str == "exit") {
            break;
        } else {
            printf("No support for this command, try again.\n");
        }
    }
}
