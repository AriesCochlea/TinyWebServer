#pragma once
#include "base/Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "ThreadPool.h"
#include "Http.h"
#include <vector>
using namespace std;


class WebServer                               //主从Reactor多线程模式
{                                            
private:
    EventLoop *mainReactor;                   //mainReactor只负责建立新连接，然后将这个连接分配给一个subReactor
    Socket listenSocket;
    Channel listenChannel;
    ThreadPool threadPool;                    //线程池
    vector<EventLoop> subReactors;            //one loop per thread
    vector< vector<Channel*> *> connectPool;  //每个EventLoop对应一个connectChannels_size大小的对象池
    //对象池，复用Channel对象和HTTP对象，省去新连接建立和销毁时new和delete的开销；
    //最关键的是为了防止在Channel类的普通成员函数中显式调用析构函数即delete this的未定义操作

public:
    WebServer(EventLoop* mainLoop, const char* ip, unsigned int port,    //主机IP和端口
              const char *user, const char *passwd, const char *dbName,  //数据库
              int threadPool_size = std::thread::hardware_concurrency(), //线程池大小
              int connectChannels_size = 10000);                         //每个对象池的大小
    ~WebServer();

    void newConnection();

    void handleReadEvent(Channel*);
    void handleWriteEvent(Channel*);
};