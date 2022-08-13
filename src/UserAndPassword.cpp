#include "UserAndPassword.h"
#include <cstring>
#include <cstdio>

UserAndPassword::UserAndPassword():fp(nullptr){
}

UserAndPassword::~UserAndPassword(){
}

int UserAndPassword::readUser(const char*user, const char * password){
    char buf[64];
    memset(buf,0,64);
    size_t u = strlen(user);
    size_t p = strlen(password);
    lock.lock();
    fp = fopen("./log/user&password.txt","w+");
    while(fgets(buf,64,fp)!=NULL){
        if(strncmp(user,buf,u)==0){
            const char *temp = buf + u +1;  //+1：表示跳过' '字符
            if(strncmp(password,temp,p)==0){
                temp += p;
                if(*temp == ' '){          //每行以" \n"结束
                    fclose(fp);
                    fp =nullptr;
                    lock.unlock();
                    return 1;              //密码正确
                }
                else{
                    fclose(fp);
                    fp =nullptr;
                    lock.unlock();
                    return 2;              //密码错误
                }
            }
        }
    }
    fclose(fp);
    fp =nullptr;
    lock.unlock();
    return 0;                              //用户名不存在
}

void UserAndPassword::addUser(const char*user, const char * password){
    lock.lock();
    fp = fopen("./log/user&password.txt","a+");
    fprintf(fp, "%s %s \n",user,password);
    fflush(fp);
    fclose(fp);
    fp =nullptr;
    lock.unlock();
}