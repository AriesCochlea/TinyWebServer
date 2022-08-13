# TinyWebServer
参考《Linux高性能服务器编程》（作者：游双）实现的C++11版Linux端Web服务器：简易的学习型项目，助初学者快速实践网络编程。


### 功能

  1.基于IO多路复用epoll（边缘触发ET）和非阻塞Socket，实现主从Reactor事件处理模型，主线程和每个工作线程都有一个事件循环；
  
  2.使用线程池减少创建、销毁工作线程的开销；使用信号量、互斥锁、条件变量等进行并发控制；
  
  3.使用主从状态机解析HTTP请求报文（HTTP1.1的GET和POST），可以向服务器请求图片和视频；使用零拷贝sendfile函数实现大文件分段传输；
  
  4.使用单例模式和阻塞队列实现异步日志系统，记录服务器运行状态；
  
  5.使用单例模式实现mysql数据库连接池，减少数据库连接建立与关闭的开销，同时实现了用户注册、登录功能；
  
  6.使用对象池，复用HTTP对象和Channel对象，减少对象在堆上构造和析构的开销。
  




### 项目启动

需要先安装和配置好Mysql数据库
```
  // 建立yourdb库
  create database yourdb;
  
  // 创建user表
  USE yourdb;
  
  CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
  )ENGINE=InnoDB;

//构建并运行
  make 
  && ./server
```





### 运行效果

注意：视频文件xxx.mp4需要你自己准备

1.手机QQ：先在Linux上使用ifconfig命令确定你的Linux所在的局域网的IPv4地址，在手机上访问http://“你的IP”:8888/即可

2.MicroSoft Edge浏览器：在浏览器地址栏输入：http://127.0.0.1:8888/
  

