#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>

#include "utility.hpp"
#include "wire/wire.hpp"
#include "device/raw_socket.hpp"
#include "application/ftp_server.hpp"


using namespace cs120;


uint32_t get_host_ip(const char *host) {
    struct hostent *server_ip = gethostbyname(host);
    return reinterpret_cast<struct in_addr *>(server_ip->h_addr_list[0])->s_addr;
}


int main(int argc, char **argv) {
//    if (argc != 3) { cs120_abort("accept 2 arguments"); }
//
//    std::shared_ptr<BaseSocket> device{new RawSocket{64}};
//
//    auto[local_ip, local_port] = parse_ip_address(argv[1]);
//
//    EndPoint remote{get_host_ip(argv[2]), 21};
//    EndPoint local{local_ip, local_port++};
//
//    FTPClient ftp_client{device, local, remote};
//
//    ftp_client.login("ftp", "");
//    ftp_client.pwd();
//    ftp_client.pasv(device, EndPoint{local_ip, local_port++});
//    ftp_client.list();
//    ftp_client.cwd("ubuntu");
//    ftp_client.pwd();
//    ftp_client.pasv(device, EndPoint{local_ip, local_port++});
//    ftp_client.list();
//    ftp_client.pasv(device, EndPoint{local_ip, local_port++});
//    ftp_client.retr("ls-lR.gz");


    if (argc != 3) { cs120_abort("accept 2 arguments"); }

    std::shared_ptr<BaseSocket> device{new RawSocket{64}};

    auto[local_ip, local_port] = parse_ip_address(argv[1]);

    EndPoint remote{get_host_ip(argv[2]), 21};
    EndPoint local{local_ip, local_port++};

    FTPClient ftp_client{device, local, remote};
    for (;;){
        using namespace std;
        string str;
        printf("ftp:");
        getline(cin,str);
        size_t pos;
        if (str.find("PWD")!=string::npos){
            ftp_client.pwd();
        }else if ((pos=str.find("CWD"))!=string::npos){
            ftp_client.cwd(str.substr(pos+4).c_str());
        }else if (str.find("PASV")!=string::npos){
            ftp_client.pasv(device,EndPoint{local_ip, local_port++});
        }else if (str.find("LIST")!=string::npos){
            if ((pos=str.find(" "))!=string::npos){
                ftp_client.list(str.substr(pos+1).c_str());
            }else{
                ftp_client.list();
            }
        } else if ((pos=str.find("RETR"))!=string::npos){
            ftp_client.retr(str.substr(pos+4).c_str());
        }else{
            printf("Not support this command, try again\n");
        }

    }



}
