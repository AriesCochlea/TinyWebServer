#pragma once
#include "base/Epoll.h"
using namespace std;

class Channel;           //Channel*

class EventLoop          //事件驱动类
{
private:
    Epoll *ep;            
    bool stop;
public:
    EventLoop();//构造函数没有参数ep。EventLoop和Epoll是“一对一”关系，构造依赖为：EventLoop→Epoll→系统调用
    ~EventLoop();

    void addChannelToEpoll(Channel*);
    void deleteChannelFromEpoll(Channel*);

    void loop();         //事件循环
};

