#include "WebServer.h"
//#include "MysqlConnectionPool.h"
#include "base/util.h"
#include "base/Log.h"
#include <functional>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h> //sendfile()
#include <iostream>
#include <utility>
#include <cstdio>
using namespace std;


WebServer::WebServer(EventLoop *mainLoop, const char* ip, unsigned int port, const char *user, const char *passwd, const char *dbName, int threadPool_size, int connectChannels_size) :
mainReactor(mainLoop), 
listenSocket(),
listenChannel(mainReactor, listenSocket.getFd()),   //mainReactor只负责listenChannel
threadPool(threadPool_size)   //主进程运行mainReactor，并创建threadPool_size个子线程用来运行subReactors
{   
    listenSocket.binder(ip, port);   
    listenSocket.listener(); 
    setnonblocking(listenChannel.getFd());         //非阻塞IO
    /*
      1.非阻塞listenSock在没有新客户端连接时，accept不阻塞，而是立即返回，且错误码errno为EWOULDBLOCK或EAGAIN。
        但本项目中事件的检测全被epoll_wait接管了，只有有新连接时代码才会执行到accept这一行。而不管是阻塞IO还是非阻塞IO，有新连接时accept都会立即返回。
      2.边缘触发ET模式会导致高并发情况下，有的客户端会连接不上。————可以使用while循环一次性accept完，直到错误码errno为EWOULDBLOCK或EAGAIN
    */
    function<void()> &&cb =  [this]{newConnection();};
    listenChannel.setReadCallback(cb);
    listenChannel.registerReading();
    listenChannel.useET();                //使用ET模式


    //MysqlConnectionPool::getInstance()->init(user,passwd,dbName);  //单例模式的数据库连接池

    subReactors.reserve(threadPool_size); 
    for(int i=0; i< threadPool_size; ++i){
        subReactors.emplace_back();        //原位构造一个EventLoop（不需要参数）对象
    }
    for(int i=0; i< threadPool_size; ++i){ //one loop per thread
        function<void()> &&subLoop = [this, i](){subReactors[i].loop();};
        threadPool.addToTasks(subLoop);    //开启所有子线程的事件循环
    }

    connectPool.reserve(threadPool_size);
    for(int i=0; i< threadPool_size; ++i){
        connectPool[i] = new vector<Channel*>();
        connectPool[i]->reserve(connectChannels_size);
        for(int j=0; j<connectChannels_size; ++j){
            (*connectPool[i])[j] = new Channel(&subReactors[i], -1);
            (*connectPool[i])[j]->getHttp();
        }
    }

    LOG_INFO("WebServer begin\n");
}

WebServer::~WebServer(){
    for(int i=0; i< connectPool.size(); ++i){
        for(int j=0; j< connectPool[i]->size(); ++j){
            delete (*connectPool[i])[j];   //delete Channel*和delete http
        }
        delete connectPool[i];             //delete vector<Channel*> *
    }
}




void WebServer::newConnection(){
    while (true){
        int connectfd = listenSocket.acceptor();  
        if(connectfd == -1){
            if(errno == EINTR)
                continue;
            if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                break;
            else
                break;
        }
        LOG_INFO("new client fd %d connected! IP:%s  Port:%d\n",connectfd, listenSocket.getClientIP(),listenSocket.getClientPort());
        setnonblocking(connectfd); //非阻塞IO

        Channel *connectChannel;
        int _random = connectfd % subReactors.size();                //调度策略：全随机。将新连接分发给一个subReactor处理
        if(connectPool[_random]->empty()){
            connectChannel = new Channel(&subReactors[_random], connectfd);
            connectPool[_random]->push_back(connectChannel);
        }
        else{
            connectChannel = connectPool[_random]->back();
            connectPool[_random]->pop_back();
            connectChannel->setFd(connectfd);
        }

        function<void()> &&cb1 = [this, connectChannel]{handleReadEvent(connectChannel);};
        function<void()> &&cb2 = [this, connectChannel]{handleWriteEvent(connectChannel);};
        connectChannel->setReadCallback(cb1);
        connectChannel->setWriteCallback(cb2);
        connectChannel->registerReading();  //注册EPOLLIN
        connectChannel->registerWriting();  //注册EPOLLOUT
        connectChannel->useET();            //注册EPOLLET
    }
}



//注：在handleReadEvent和handleWriteEvent中delete connectChannel;相当于在类的成员函数中delete this;即删除自己，是未定义行为！
//本项目使用对象池来复用Channel对象和HTTP对象，以解决上述未定义行为

void WebServer::handleReadEvent(Channel *connectChannel){
    Http *httpConn = connectChannel->getHttp();
    //由于使用ET模式，要一次性读取完旧事件的数据，直到errno == EAGAIN || errno == EWOULDBLOCK 跳出循环，再一次性读取新事件的数据
    while(true){ 
        char buf[1025];buf[1024] = '\0';
        ssize_t ret = recv(connectChannel->getFd(), buf, 1024, 0);
        if(ret > 0){
            httpConn->readBuffer.append(buf);
        } 
        else if(ret == -1){
            if( errno == EINTR){     //客户端正常中断
                continue;
            }
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){//对非阻塞IO而言，这个条件表示旧数据全部读取完毕
                //LOG_INFO("read once");
                STATUS_CODE ret = httpConn->readRequestMessage();
                if(ret == CONTINUE)          //读取到的请求报文不完整
                    break;                   //保留现场，且不生成响应报文
                /*
                    有的浏览器的POST请求会分两次发送：第一次发送请求头，等待服务器发送CONTINUE（即状态码100）；第二次才发送请求体。
                    而有的浏览器的POST请求不会分成两次发送（真是服了这些乱七八糟的协议！）。本项目支持的是后者。
                */
                httpConn->readBuffer = "";  
                if(httpConn->bytes_left > 0) //上次调用handleWriteEvent()时没有把响应报文传完
                    break; //忽略此次的请求报文。传输大文件会导致该HTTP连接的其他请求无法及时处理。
                else{
                    httpConn->setResponseMessage(ret); 
                    httpConn->reDone = true;   
                    //LOG_INFO("ResponseMessage done");
                    break;
                }
            }
            else {
                LOG_INFO("the server disconnected to client fd %d \n", connectChannel->getFd());
                connectChannel->deleteHttpConnection();
                //delete connectChannel;       //在成员函数里delete this（显式调用析构函数）是未定义行为！
                break;
            }
        }
        else if(ret == 0){          //EOF，客户端断开连接
            LOG_INFO("EOF, client fd %d disconnected!\n", connectChannel->getFd());
            connectChannel->deleteHttpConnection();
            break;
        }
    }
} 


void WebServer::handleWriteEvent(Channel *connectChannel){
    Http *httpConn = connectChannel->getHttp();
    if(httpConn->reDone == false || httpConn->bytes_left <= 0)
        return;

    while(httpConn->bytes_left > 0){                 
        ssize_t bytes_sent;
        if(httpConn->bytes_left <= httpConn->fileStat.st_size){
            off_t offset =  httpConn->fileStat.st_size - httpConn->bytes_left;
            bytes_sent = sendfile(connectChannel->getFd(), httpConn->fileFd, &offset, 1024);
        }     
        else{
            off_t offset =  httpConn->fileStat.st_size + httpConn->writeBuffer.size() - httpConn->bytes_left;
            bytes_sent = send(connectChannel->getFd(), httpConn->writeBuffer.c_str() + offset, 
                              httpConn->bytes_left - httpConn->fileStat.st_size,  0);
        }     
        if(-1 == bytes_sent){
            if(errno == EINTR)//客户端正常中断
                continue;
            else if((errno == EAGAIN) || (errno == EWOULDBLOCK) ){ //由于使用ET模式，要一次性写到缓冲区满为止
                //LOG_INFO("write once");
                break;//大文件无法一次传完，客户端可能会反复发送同样的请求。这些重复请求和该连接的其他新请求都在handleReadEvent()函数中被忽略了。
               /*HTTP协议标准肯定是有对应的解决办法，且需要客户端和服务端双方遵守，但本项目只是HTTP协议的简陋原型，不考虑那么多。
                 对于自己设计的协议，比如可以让每个请求报文带上序号，响应报文分成序号相同的多份来实现分段响应，
                 客户端将序号相同的响应报文的响应体合并成一个，可实现异步响应：比如后到的请求由于响应体很小可以先被响应。
                 再比如，客户端可以给需要传输大文件的请求新建一条TCP连接，服务器可以用一个新线程接收这个连接。
               */
            }
            else {
                LOG_INFO("the server disconnected to client fd %d\n", connectChannel->getFd());
                connectChannel->deleteHttpConnection();
                break;
            }
        }
        httpConn->bytes_left -= bytes_sent;
    }
    if(httpConn->bytes_left <= 0){
        httpConn->writeBuffer = "";
        httpConn->bytes_left = 0;
        httpConn->reDone = false;
        close(httpConn->fileFd);
        httpConn->fileFd = -1;
        LOG_INFO("Response done\n");

        if(httpConn->keepAlive == false){ //短连接不是HTTP/1.1默认的
            LOG_INFO("the server disconnected to client fd %d\n", connectChannel->getFd());
            connectChannel->deleteHttpConnection();
        }
    }
}

