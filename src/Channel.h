#pragma once
#include "Http.h"
#include <sys/epoll.h>
#include <functional>
using namespace std;

class EventLoop;     //EventLoop *loop;

class Channel        //封装了一个文件描述符及其关心的事件、epoll返回的事件、事件的处理函数等
{
private:
    EventLoop *loop; //EventLoop &loop;也行，本质上一样，64位机上类对象的成员指针变量或引用都是占用8字节空间
    int fd;
    bool inEpoll;
    uint32_t events;
    uint32_t revents;
    function<void()> readCallback;
    function<void()> writeCallback;
public:
     
    Channel(EventLoop *_loop, int _fd);
    ~Channel();

    Http *http;                                //可以复用
    Http* getHttp();
    void deleteHttpConnection();
    
    void setReadCallback(function<void()> &);  //引用传参，减少一次复制
    void setWriteCallback(function<void()> &); //引用传参，减少一次复制
    void registerReading();                    //向epoll注册fd上的“读”事件（包括普通数据和优先数据可读）
    void registerWriting();                    //向epoll注册fd上的“写”事件（包括普通数据和优先数据可写）
    void useET();                              //启用ET模式

    void handleEvents();                       //处理epoll_wait返回的事件

    int getFd();
    void setFd(int fd);                        //更新成员变量
    bool getInEpoll();
    uint32_t getEvents();
    uint32_t getRevents();
    void setInEpoll(bool in=true);
    void setEvents(uint32_t);
    void setRevents(uint32_t);

};

