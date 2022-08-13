#include "Log.h"
#include <string>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <sys/time.h>
using namespace std;

Log::Log():isAsync(false), fp(nullptr), linesCount(0), openLog(false), asyncWriteLog(nullptr){
    init(true, "./log/log.txt", true);
}
Log::~Log(){
    openLog = false; //原子变量不用加锁
    if(asyncWriteLog!=nullptr){
        asyncWriteLog->join();
        delete asyncWriteLog;
    }
    if(fp!=nullptr) //在delete asyncWriteLog;之后执行，所以也不用加锁
        fclose(fp);
}


void Log::init(bool open_log, const char *fileName, bool is_async, int max_lines){
    if(asyncWriteLog!=nullptr){
        openLog == false;
        asyncWriteLog->join();
        delete asyncWriteLog;
        asyncWriteLog = nullptr;
    }
    openLog = open_log;
    if(openLog==false || fileName==nullptr)
        return;
    maxLines = max_lines;
    isAsync = is_async;

    if(isAsync ==  true){
        asyncWriteLog = new thread([this]{
        string singleLog;
        while(true){
            {
                unique_lock<mutex> u(mtx);
                while(openLog && blockQueue.empty())
                    cv.wait(u);  
                if(!openLog && blockQueue.empty())
                    return;
                else{
                    singleLog = blockQueue.front();
                    blockQueue.pop();
                }
                fputs(singleLog.c_str(), fp);
                fflush(fp);
            }
        }
        });
    }


    time_t t = time(NULL);
    struct tm *my_tm = localtime(&t);
    today = my_tm->tm_mday; 

    const char * p = strrchr(fileName, '/');
    char tempName[600];
    if(p==NULL){
        strcpy(logName, fileName);
        //dirName相当于./
        snprintf(tempName, 600, "%d_%02d_%02d_%s", my_tm->tm_year + 1900, my_tm->tm_mon + 1, my_tm->tm_mday, logName);
    }
    else{
        strcpy(logName, p + 1);
        strncpy(dirName, fileName, p + 1 - fileName);
        snprintf(tempName, 600, "%s%d_%02d_%02d_%s", dirName, my_tm->tm_year + 1900, my_tm->tm_mon + 1, my_tm->tm_mday, logName);
    }


    {
        unique_lock<mutex> u(mtx);
        if(fp!=nullptr)
            fclose(fp);
        fp = fopen(tempName, "a");
        if(fp==nullptr){
            perror("logfile fopen error");
            exit(EXIT_FAILURE);
        }
    }

}



void Log::writeLog(int level, const char *format, ...){
    if(openLog==false || fp==nullptr)
        return;

    struct timeval now = {0, 0}; //#include <sys/time.h>
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char temp[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(temp, "[debug]:");
        break;
    case 1:
        strcpy(temp, "[info]:");
        break;
    case 2:
        strcpy(temp, "[warn]:");
        break;
    case 3:
        strcpy(temp, "[erro]:");
        break;
    default:
        strcpy(temp, "[info]:");
        break;
    }
        
    ++linesCount;
    if(my_tm.tm_mday!=today || linesCount % maxLines ==0){
        char new_log[640];
        char date[128];
        snprintf(date, 128, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);
        if (my_tm.tm_mday!=today){ //服务器一般不停机，每天凌晨12点建立一个全新的日志文件
            snprintf(new_log, 640, "%s%s%s", dirName, date, logName);
            today = my_tm.tm_mday;
            linesCount = 0;
         }
        else{ //linesCount % maxLines ==0 
            snprintf(new_log, 640, "%s%s%s.%d", dirName, date, logName, linesCount.load()/maxLines.load());
        }
        {
            unique_lock<mutex> u(mtx);
            if(fp!=nullptr)
                fclose(fp);    
            fp = fopen(new_log, "a"); //建立一个全新的日志文件
            if(fp==nullptr){
                perror("newlogfile fopen error");
                exit(EXIT_FAILURE);
            }
        }
    }


    char buf[8192];
    va_list vaList;
    va_start(vaList, format);
    //每行日志的具体时间内容格式
    int n = snprintf(buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, temp);
    
    int m = vsnprintf(buf + n, 8190 -n, format, vaList); //m和n不包括字符串末尾的空字符
    buf[n + m] = '\n';
    buf[n + m + 1] = '\0';
    va_end(vaList);
    
    if(isAsync == true){
        {
            unique_lock<mutex> u(mtx);
            blockQueue.emplace(buf);
        }
        cv.notify_one(); //不加锁
    }
    else{
        fputs(buf,fp);
        fflush(fp);
    }
}

