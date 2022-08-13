#include "EventLoop.h"
#include "Channel.h"
#include "base/Log.h"
#include <vector>
using namespace std;

EventLoop::EventLoop():stop(false), ep(nullptr){
    ep = new Epoll();
}
EventLoop::~EventLoop(){
    stop == true;
    LOG_INFO("EventLoop stop");
    if(ep != nullptr)
        delete ep;
}

void EventLoop::addChannelToEpoll(Channel *ch){
    ep->addChannelToEpoll(ch);
}
void EventLoop::deleteChannelFromEpoll(Channel *ch){
    ep->deleteChannelFromEpoll(ch);
}


void EventLoop::loop(){
    while(!stop){
        ep->poll();
        while(!ep->channels.empty()){
            ep->channels.front()->handleEvents();
            ep->channels.pop();
        }
    }
}

