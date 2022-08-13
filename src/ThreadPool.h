#pragma once
#include <utility> 
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

class ThreadPool
{
private:
    vector<thread> threads;
    queue<function<void()> > tasks; 
    mutex tasks_mtx;
    condition_variable cv;
    bool stop;
public:
    ThreadPool(int size); //默认size最好设置为thread::hardware_concurrency()
    ~ThreadPool();

    void addToTasks(function<void()> &);
    /*
    template<typename F, typename... T>
        void addToTasks(F&& f, T&&... args);*/
};
/*
//C++不支持模版的分离编译，不能放在.cpp文件里 ————个人不太喜欢用模板
template<typename F, typename... T>
void ThreadPool::addToTasks(F&& f, T&&... args) {                       //万能引用：左值引用和右值引用均可
    //bind能自动解参数包，无需手动递归解包
    function<void()> task = bind(forward<F>(f), forward<T>(args)...) ;  //对参数进行完美转发
    {
        unique_lock<mutex> lock(tasks_mtx);
        if(!stop)
            tasks.push(&task);
    }
    cv.notify_one();
}
*/