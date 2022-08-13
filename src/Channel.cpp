#include "Channel.h"
#include "EventLoop.h"
#include "base/Epoll.h"
#include <unistd.h>
using namespace std;

Channel::Channel(EventLoop *_loop, int _fd) : loop(_loop), fd(_fd),
         inEpoll(false), events(0), revents(0), http(nullptr){}

Channel::~Channel(){
    if(http!=nullptr){
        delete http;
    }
    if(fd !=-1){
        loop->deleteChannelFromEpoll(this);
        //close(fd); listenSocket的fd不在这里close
    }
}



Http* Channel::getHttp(){
    if(http == nullptr){
        http = new Http();
    }
    return http;
}
void Channel:: deleteHttpConnection(){
    if(fd !=-1){
        loop->deleteChannelFromEpoll(this);
        close(fd);//connetSocket的fd和由setFd()设置的fd在这里close
        fd = -1;
    }
    http->init();             //复用，不删除
    http->keepAlive = false;
    http->filePath = doc_root;
    http->fileFd = -1;
    http->bytes_left = 0;
    http->readBuffer = "";
    http->writeBuffer = "";
    http->reDone = false;
}



void Channel::setReadCallback(function<void()> &cb){ //引用
    readCallback = cb;                               //复制
}
void Channel::setWriteCallback(function<void()> &cb){ //引用
    writeCallback = cb;                               //复制
}


void Channel::registerReading(){
    events |= EPOLLIN;
    loop->addChannelToEpoll(this);
}
void Channel::registerWriting(){
    events |= EPOLLOUT;
    loop->addChannelToEpoll(this);
}
void Channel::useET(){
    events |= EPOLLET;
    loop->addChannelToEpoll(this);
}



void Channel::handleEvents(){
    if(revents & EPOLLIN){
            readCallback();
    }
    if(revents & EPOLLOUT){
            writeCallback();
    }
    revents = 0;
}



int Channel::getFd(){
    return fd;
}
void Channel::setFd(int fd){
    this->fd = fd;
    events = 0;
    revents = 0;
    inEpoll = false;
}
bool Channel::getInEpoll(){
    return inEpoll;
}
uint32_t Channel::getEvents(){
    return events;
}
uint32_t Channel::getRevents(){
    return revents;
}
void Channel::setInEpoll(bool in){
    inEpoll = in;
}
void Channel::setEvents(uint32_t _ev){
    events = _ev;
}
void Channel::setRevents(uint32_t _ev){
    revents = _ev;
}


