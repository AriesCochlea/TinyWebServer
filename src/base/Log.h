#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <cstdarg>
using namespace std;


class Log
{
private:
    Log();
    ~Log();
    //static Log log;            //单例模式的“饿汉”式
public:
    static Log* getSingleton(){  //单例模式的“懒汉”式
        static Log log;          //从C++11开始静态局部对象不用加锁也能保证多线程下安全
        return &log;
    }

    void init(bool open_log=false, const char *fileName=nullptr, bool is_async = true, int max_lines = 1000000);
    void writeLog(int level, const char *format, ...);

private:
    mutex mtx;                   //给blockQueue和fp加锁
    queue<string> blockQueue;    //异步日志：将所写的日志内容先存入阻塞队列，写线程从阻塞队列中取出内容，写入文件
    atomic <bool> isAsync;       //是否异步
    FILE *fp;                    //打开log的文件指针
    char dirName[256];           //路径名，子线程不会访问，所以不用加锁
    char logName[256];           //文件名，子线程不会访问，所以不用加锁
    atomic<int> maxLines;        //日志最大行数
    atomic<int> linesCount;      //日志行数记录
    atomic<int> today;           //记录当前时间是哪一天
    atomic<bool> openLog;        //是否开启/关闭日志
    thread *asyncWriteLog;
    condition_variable cv;
};
//Log Log::log;                  //配合“饿汉”式使用


//__VA_ARGS__是一个可变参数的宏，定义时宏定义中参数列表的最后一个参数为省略号，在实际使用时会发现有时会加##，有时又不加。
#define LOG_DEBUG(format, ...)  {Log::getSingleton()->writeLog(0, format, ##__VA_ARGS__); }
#define LOG_INFO(format, ...)  {Log::getSingleton()->writeLog(1, format, ##__VA_ARGS__); }
#define LOG_WARN(format, ...)  {Log::getSingleton()->writeLog(2, format, ##__VA_ARGS__); }
#define LOG_ERROR(format, ...)  {Log::getSingleton()->writeLog(3, format, ##__VA_ARGS__); }
//Debug，调试代码时的输出，在系统实际运行时，一般不使用。
//Warn，这种警告与调试时终端的warning类似，同样是调试代码时使用。
//Info，报告系统当前的状态，当前执行的流程或接收的信息等。
//Error，输出系统的错误信息。