#pragma once
#include <arpa/inet.h>

class Socket
{
public:
    Socket();
    ~Socket();

    int connector(const char* ip, uint16_t port); //客户端三次握手发生在这个阶段  ————用于客户端
                                                  //参数是服务器用于监听socket的IP和Port
    void binder(const char* ip, uint16_t port);   //如binder("127.0.0.1", 8888);
    void listener();                              //服务端三次握手发生在这个阶段。————用于服务端
    int acceptor();                               //服务端返回连接socketfd     ————用于服务端

    int getFd();                                  //服务端返回监听socketfd，客户端返回连接socketfd
    void setFd(int fd);
    char* getClientIP();                          //————用于服务端
    uint16_t getClientPort();                     //————用于服务端
private:
    int fd;
    struct sockaddr_in addr;
    struct sockaddr_in peer_addr;                 //对端的地址信息。
};

