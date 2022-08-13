#pragma once
#include <mysql/mysql.h> //MYSQL，mysql_init()， mysql_real_connect()， mysql_close()
#include <semaphore.h>   //#include <semaphore>需要C++20支持
#include <queue>
#include <mutex>   
using namespace std;


/*
127.0.0.1相当于使用网络去访问本机
localhost不用联网即可访问本地服务权限
*/


class MysqlConnectionPool{
public:
    static MysqlConnectionPool* getInstance(){
        static MysqlConnectionPool mysqlConnnectionPool;  //局部静态变量单例模式之懒汉式：C++11起能保证线程安全，无需加锁
        return &mysqlConnnectionPool;
    }

    //参数依次为：数据库用户名、密码、库名、端口、主机名、池容量
    //user：用户的MySQL登录ID
    //password：用户的密码
    //dbName：数据库名称
    //port：  如果port不是0，其值将用作TCP/IP连接的端口号。port为0的话，使用mysql的默认tcp/ip端口3306。
    //host：必须是主机名或IP地址。如果“host”是NULL或字符串"localhost"，连接将被视为与本地主机的连接
    void init(const char *user, const char *password, const char *dbName, unsigned int port = 3306, const char *host = "localhost",int max_conn = 16);
                                         //经测试：max_conn在笔者电脑上最大为152，即最多只能预开152个mysql连接
    MYSQL *getConnection();				 //从数据库连接池中返回一个可用连接
    void freeConnection(MYSQL *conn);    //释放当前使用的连接conn
	void destroyPool();					 //销毁数据库连接池

private:
    MysqlConnectionPool();
    ~MysqlConnectionPool();

    //int maxConn;
    //int usedConn;
    //int freeConn;
    queue<MYSQL*> connQueue;            //数据库连接池
    mutex locker;                       //用互斥锁锁住connQueue
    sem_t sem;                          //也可以改用条件变量来实现：条件为freeConn > 0（或!connQueue.empty()）
                                        //大部分情况下，信号量综合了互斥锁和条件变量的功能  
};

/*若只用信号量来实现：（不用互斥锁和条件变量）
增加类成员：
    sem_t sem_connQueue;               //sem_connQueue是连接池的计数器（为1），而上面的sem是连接池中freeConn的计数器

对应的成员函数改为：

void MysqlConnectionPool::init(......){
    ......
    sem_init(&sem_connQueue, 0, 1);   //连接池的计数器（为1）
    ......
}

MYSQL* MysqlConnectionPool::getConnection(){
    if(connQueue.empty())
        return nullptr;
    MYSQL *mysql;
    sem_wait(&sem);
    sem_wait(&sem_connQueue);         //取代互斥锁的功能
    mysql = connQueue.front();
    connQueue.pop();
    sem_post(&sem_connQueue);         //取代互斥锁的功能

    return mysql;
}

void MysqlConnectionPool::freeConnection(MYSQL* conn){
    if(conn == nullptr)
        return;
    sem_wait(&sem_connQueue);         //取代互斥锁的功能
    connQueue.push(conn);
    sem_post(&sem_connQueue);         //取代互斥锁的功能
    sem_post(&sem);
}

void MysqlConnectionPool::destroyPool(){
    sem_wait(&sem_connQueue);         //取代互斥锁的功能
    while(!connQueue.empty()){
        MYSQL *temp = connQueue.front();
        connQueue.pop();
        mysql_close(temp);
    }
    mysql_library_end(); 
    sem_post(&sem_connQueue);         //取代互斥锁的功能
}

*/