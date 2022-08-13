#include "Socket.h"
#include "util.h"
#include <unistd.h>
#include <sys/socket.h>
#include <cstdio>



Socket::Socket() : fd(-1){
    fd = socket(AF_INET, SOCK_STREAM, 0);
    errif(fd == -1, "socket create error");
}
Socket::~Socket(){
    if(fd != -1){
        close(fd);
    }
}



int Socket::connector(const char* ip, uint16_t port){
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_addr.s_addr = inet_addr(ip);
    peer_addr.sin_port = htons(port);    
    int ret = connect(fd, (const struct sockaddr*)&peer_addr, (socklen_t)sizeof(peer_addr));
    //errif(ret == -1, "socket connect error");//客户端在ET模式下使用非阻塞IO应该删掉此行
    return ret;
}



void Socket::binder(const char* ip, uint16_t port){
    addr.sin_family = AF_INET;
    //addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    errif(bind(fd, (const struct sockaddr*)&addr, (socklen_t)sizeof(addr)) == -1, "socket bind error");
}
void Socket::listener(){
    errif(listen(fd, SOMAXCONN) == -1, "socket listen error");
}


int Socket::acceptor(){
    socklen_t socklen=sizeof(peer_addr); 
    int connfd = accept(fd,(struct sockaddr*)&peer_addr, &socklen);
    //errif(connfd == -1, "socket accept error");                  //ET模式下使用非阻塞IO应该删掉此行
    return connfd;
}




int Socket::getFd(){
    return fd;
}
void Socket::setFd(int fd){
    this->fd = fd;
}
char* Socket::getClientIP(){
    return inet_ntoa(peer_addr.sin_addr);
}
uint16_t Socket::getClientPort(){
    return ntohs(peer_addr.sin_port);
}
