#include "MysqlConnectionPool.h"
#include "base/util.h"
#include "base/Log.h"

MysqlConnectionPool::MysqlConnectionPool()//:usedConn(0), freeConn(0)
{}

MysqlConnectionPool::~MysqlConnectionPool(){
    destroyPool();
}

void MysqlConnectionPool::init(const char *user, const char *password, const char *dbName, unsigned int port, const char *host, int max_conn){
    for(int i=0; i< max_conn; ++i){
        MYSQL *mysql= nullptr;
        mysql = mysql_init(mysql);                                                     //#include <mysql/mysql.h> 
        errif(mysql == nullptr, "MySql init error");
        mysql = mysql_real_connect(mysql, host, user,password,dbName,port,nullptr,0);  //#include <mysql/mysql.h> 
        errif(mysql == nullptr, "MySql connect error");
        connQueue.push(mysql);
        //++ freeConn;
    }
    //maxConn = max_conn;
    sem_init(&sem, 0, max_conn);
    LOG_INFO("MysqlConnectionPool created");
}




MYSQL* MysqlConnectionPool::getConnection(){
    if(connQueue.empty())
        return nullptr;
    MYSQL *mysql;
    sem_wait(&sem); //信号量原子地减1，为0则阻塞等待
    locker.lock();
    mysql = connQueue.front();
    connQueue.pop();
    //++ usedConn;
    //-- freeConn;
    locker.unlock();
    return mysql;
}


void MysqlConnectionPool::freeConnection(MYSQL* conn){
    if(conn == nullptr)
        return;
    locker.lock();
    connQueue.push(conn);
    //-- usedConn;
    //++ freeConn;
    locker.unlock();
    sem_post(&sem); //信号量原子地加1
}

void MysqlConnectionPool::destroyPool(){
    locker.lock();
    while(!connQueue.empty()){
        MYSQL *temp = connQueue.front();
        connQueue.pop();
        mysql_close(temp);  //#include <mysql/mysql.h> 
    }
    mysql_library_end();    //#include <mysql/mysql.h> 
    locker.unlock();

    LOG_INFO("MysqlConnectionPool destroyed");
}