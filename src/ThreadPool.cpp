#include "ThreadPool.h"
#include "base/Log.h"
using namespace std;

ThreadPool::ThreadPool(int size): stop(false){
    for(int i = 0; i < size; ++i){
        threads.emplace_back([this]{ //C++11的thread对象在创建后就已经开始运行线程了
            while(true){
                function<void()> task;
                {   //tasks_mtx.lock();
                    std::unique_lock<std::mutex> lock(tasks_mtx);
                    while(stop==false && tasks.empty()){ 
                        cv.wait(lock);
                    }//或写成：cv.wait(lock, [this](){return stop==true || !tasks.empty();} );
                    if(stop==true && tasks.empty()) {
                        return; //tasks_mtx.unlock();return;
                    }
                    task = tasks.front();
                    tasks.pop();
                    //tasks_mtx.unlock();
                }
                //每个线程对象的每层循环从多个线程共享的任务队列tasks里取出任务并执行
                task();//one loop per thread，每个task都是一个loop（而loop里有个循环while），即主从Reactor模式下的工作线程不会睡眠，因为task()不会执行完
                       //所以上面的加锁开销只在刚开始时需要计算。其实连ThreadPool.h里的tasks队列也不需要（当然也就不需要加锁了），此处只是为了方便实现其他模式时不用改代码。
            }  
        });
    }
    LOG_INFO("ThreadPool created");
}

ThreadPool::~ThreadPool(){
    {
        std::unique_lock<std::mutex> lock(tasks_mtx);
        stop = true;
    }
    cv.notify_all();
    for(auto &th : threads){
        if(th.joinable())
            th.join();          //主线程（运行事件驱动EventLoop）等待线程池中每个线程执行完
    }
    LOG_INFO("ThreadPool destroyed");
}


void ThreadPool::addToTasks(function<void()> &func){  //引用
    {
        std::unique_lock<std::mutex> lock(tasks_mtx);
        if(!stop)
            tasks.push(func);                         //复制
    }
    cv.notify_one();                                  //不加锁
}

