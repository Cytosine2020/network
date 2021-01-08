//
// Created by zhao on 2021/1/3.
//

#include "ftp_client.h"
#include <string>
#include <cstring>
#include <libnet.h>
#include <fstream>
#include <utility>

using namespace cs120;

ftp_client::ftp_client(char *servername, std::shared_ptr<BaseSocket> sock1, std::shared_ptr<BaseSocket>sock2):data_port(0),connect(nullptr),data_connect(
        nullptr){
    lan = sock1;
    data_lan=std::move(sock2);
    Array<uint8_t> buffer{2048};
//    struct sockaddr_in client_command, client_data;
//    struct sockaddr client_arg;
//    client_command.sin_family = AF_INET;
//    client_command.sin_port = htons(1025);
//    client_command.sin_addr.s_addr = INADDR_ANY;
//    client_data.sin_family = AF_INET;
//    client_data.sin_port = htons(1026);
//    client_data.sin_addr.s_addr = INADDR_ANY;
//
//
//    server.sin_family = AF_INET;
//
//    server.sin_port = htons(21);
//
//    client_socket = socket(AF_INET, SOCK_STREAM, 0);
//
//    if (bind(client_socket, (struct sockaddr *) (&client_command), sizeof(struct sockaddr_in)) == -1) {
//        printf("bind error: %s(errno: %d)\n", strerror(errno), errno);
//        exit(-1);
//    }
//    data_socket = socket(AF_INET, SOCK_STREAM, 0);
//    if (bind(data_socket, (struct sockaddr *) (&client_data), sizeof(struct sockaddr_in)) == -1) {
//        printf("bind error: %s(errno: %d)\n", strerror(errno), errno);
//        exit(-1);
//    }
//    if (client_socket == -1) {
//
//        printf("Can't create socket");
//
//        exit(-1);
//    }

    struct hostent *server_ip = gethostbyname(servername);

    if (!server_ip) {

        printf("Can't get server's IP");

        exit(-1);

    }

    uint32_t server_address = (*(struct in_addr *) server_ip->h_addr_list[0]).s_addr;
    uint32_t local_address=get_local_ip();
    struct EndPoint server{server_address,21};
    struct EndPoint local{local_address,1025};
    auto *server_connect=new TCPClient(sock1,64,local,server);
    connect=server_connect;

    size_t size=connect->recv(buffer[Range{}]);
    if (size <= 0) {
        printf("nothing recv");
    } else {
        printf("%.*s",(int) size,buffer.begin());
    }
}

int ftp_client::login(char *user_name, char *password) {
    char send_buff[BUFF_LEN];
    Array<uint8_t> read;
    sprintf(send_buff, "USER %s\r\n", user_name);
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s",(int) size, buffer.begin());
        }
    }
    memset(send_buff,0,BUFF_LEN);

    sprintf(send_buff, "PASS %s\r\n", password);
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s",(int) size, buffer.begin());
        }
    }

    return 1;
}

int ftp_client::PWD() {
    char send_buff[BUFF_LEN];
    sprintf(send_buff, "PWD\r\n");
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s",(int) size, buffer.begin());
        }
    }
    return 1;
}

int ftp_client::CWD(char *pathname) {
    char send_buff[BUFF_LEN];
    sprintf(send_buff, "CWD %s\r\n", pathname);
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s", (int)size, buffer.begin());
        }
    }
    return 1;
}

int ftp_client::PASV() {
    char send_buff[BUFF_LEN];
    char read_buff[BUFF_LEN];
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s",(int) size, buffer.begin());
            for (size_t i = 0; i < buffer.size(); ++i) {
                read_buff[i]=buffer[i];
            }
        }
    }

    std::regex reg(".*\\d+\\s.*,(\\d+),(\\d+)\\)\\.\\r\\n");
    std::cmatch m;
    bool flag1 = std::regex_match(read_buff, m, reg);
    if (flag1) {
        data_port = stoi(m.str(1)) * 256 + stoi(m.str(2));
    } else {
        for (size_t i = 0; i < strlen(read_buff); ++i) {
            printf("%x\n", read_buff[i]);
        }

        return -1;
    }
    std::regex reg2(".*\\d+\\s.*\\((\\d+),(\\d+),(\\d+),(\\d+).*");
    std::string str = std::regex_replace(read_buff, reg2, "$1.$2.$3.$4");

    uint32_t server_address = inet_addr(str.c_str());
    uint32_t local_address=get_local_ip();
    struct EndPoint server{server_address,data_port};
    struct EndPoint local{local_address,1026};
    auto *server_connect=new TCPClient(data_lan,64,local,server);
    data_connect=server_connect;
    return 1;
}


int ftp_client::LIST(char *listname) {
    char send_buff[BUFF_LEN];
    char read_buff[BUFF_LEN];
    if (strlen(listname) == 0) {
        sprintf(send_buff, "LIST\r\n");
    } else {
        sprintf(send_buff, "LIST %s\r\n", listname);
    }
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s", (int)size, buffer.begin());
            for (size_t i = 0; i < buffer.size(); ++i) {
                read_buff[i]=buffer[i];
            }
        }
    }

    memset(read_buff, 0, BUFF_LEN);

    {
        Array<uint8_t> buffer{2048};
        size_t size = data_connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else {
            printf("%.*s",(int) size, buffer.begin());
            for (size_t i = 0; i < buffer.size(); ++i) {
                read_buff[i]=buffer[i];
            }
        }
    }
    return 1;
}

int ftp_client::RETR(char *filename) {
    char send_buff[BUFF_LEN];
    sprintf(send_buff, "RETR %s\r\n", filename);
    {
        Slice<uint8_t> send{reinterpret_cast<const unsigned char *>(send_buff), strlen(send_buff)};
        connect->send(send);
    }
    {
        Array<uint8_t> buffer{2048};
        size_t size = connect->recv(buffer[Range{}]);
        if (size <= 0) {
            printf("nothing recv");
        } else if (size==1500){
            std::fstream file("download_file",std::ios::out|std::ios::binary);
            if(!file){
                printf("can't open file");
                return -1;
            }

            while(size==1500){
                file<<buffer.begin();
                size = connect->recv(buffer[Range{}]);
            }
            for (size_t i = 0; i < size; ++i) {
                file<<buffer.begin()[i];
            }
        }
    }
    return 1;
}

int main(){
    std::shared_ptr<BaseSocket> sock1(new RawSocket{64});
    std::shared_ptr<BaseSocket> sock2(new RawSocket{64});
    ftp_client ftpClient("ftp.sjtu.edu.cn",sock1,sock2);
    ftpClient.login("ftp","");
    ftpClient.PWD();
    ftpClient.PASV();
    ftpClient.LIST("");

}

