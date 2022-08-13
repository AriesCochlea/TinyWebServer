//笔者把项目从ubuntu迁移到wsl上时，遇到mysql总是无法连接的问题，暂时找不到解决办法，于是就有了这个文件，毕竟先让项目跑起来才是关键
//本文件只影响WebServer.cpp和Http.cpp
//如果你的mysql API：mysql_real_connect()能正常连接，则不需要包含此头文件，并略微修改一下WebServer.cpp（第36行）和Http.cpp（第392行起）


//将用户名和密码存在文本文件中：
#pragma once
#include <mutex>
using namespace std;

class UserAndPassword
{
public:
	static UserAndPassword* getInstance(){
        static UserAndPassword userAndPassword;  //局部静态变量单例模式之懒汉式：C++11起能保证线程安全，无需加锁
        return &userAndPassword;
    }
	void addUser(const char*user, const char * password);
	int readUser(const char*user, const char * password);
private:
	UserAndPassword();
	~UserAndPassword();
	FILE *fp;
	mutex lock;
};
