#include "Epoll.h"
#include "../Channel.h"
#include "util.h"
#include <unistd.h>
using namespace std;


Epoll::Epoll() : epfd(-1){
    epfd = epoll_create1(0);
    errif(epfd == -1, "epoll create error");
}
Epoll::~Epoll(){
    if(epfd != -1){
        close(epfd);
        //epfd = -1;
    }
}

void Epoll::addChannelToEpoll(Channel *channel){ 
    struct epoll_event ev;
    ev.data.ptr = channel;                           //注册时用的指针，epoll_wait返回时也是指针
    ev.events = channel->getEvents();
    if(!channel->getInEpoll()){
        errif(epoll_ctl(epfd, EPOLL_CTL_ADD, channel->getFd(), &ev) == -1, "epoll add error");
        channel->setInEpoll(true);
    }
    else{
        errif(epoll_ctl(epfd, EPOLL_CTL_MOD, channel->getFd(), &ev) == -1, "epoll modify error");
    }
}
void Epoll::deleteChannelFromEpoll(Channel *channel){
    if(channel->getInEpoll()){
        errif(epoll_ctl(epfd, EPOLL_CTL_DEL, channel->getFd(), nullptr) == -1, "epoll delete error");
        channel->setInEpoll(false);
    }
}



void Epoll::poll(int timeout){
    int nfds = epoll_wait(epfd, revents, MAX_EVENTS, timeout);
    errif(nfds == -1, "epoll_wait error");
    for(int i = 0; i < nfds; ++i){
        Channel *ch = (Channel*)revents[i].data.ptr;  //注册时用的指针，epoll_wait返回时也是指针
        ch->setRevents(revents[i].events);
        channels.push(ch);
    }
}
