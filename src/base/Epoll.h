/*macOS里并没有epoll，取而代之的是FreeBSD的kqueue，功能和使用都和epoll很相似
  跨平台需要修改最底层的Epoll类，而不需要改动上层的EventLoop类*/

#pragma once

#include <sys/epoll.h>
#include <queue>
using namespace std;

#define MAX_EVENTS 100000


class Channel;          //Channel*

class Epoll{  
private:
    int epfd;
    struct epoll_event revents[MAX_EVENTS];  //用于接收每次epoll_wait返回的事件
public:

    queue <Channel*> channels;               //对应revents的channels
    Epoll();
    ~Epoll();
    void addChannelToEpoll(Channel*);
    void deleteChannelFromEpoll(Channel*);
    
    void poll(int timeout = -1);            //封装了epoll_wait
};