#include "src/EventLoop.h"
#include "src/WebServer.h"

int main() {
    const char * ip    ="127.0.0.1";
    unsigned int port  = 8888;           //监听套接字绑定的端口。注：主流HTTP端口号是80
    const char *user   ="root";          //改成你自己的mysql用户名
    const char *passwd = NULL;           //改成你自己的mysql用户名对应的密码
    const char *dbName ="yourdb";        //改成你想连接的数据库名

    EventLoop loop;
    WebServer server(&loop, ip, port, user, passwd, dbName);
    loop.loop();
    return 0;
}