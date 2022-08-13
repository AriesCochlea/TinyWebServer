/*HTTP报文分为请求报文和响应报文两种，浏览器端向服务器发送的为请求报文，服务器处理后返回给浏览器端的为响应报文。
  HTTP请求方法共9种：HTTP1.0定义了三种请求方法： GET, POST， HEAD。
  HTTP1.1新增了六种请求方法：OPTIONS、PUT、PATCH、DELETE、TRACE、CONNECT。
  在实际应用中常用的是 get 和 post，其他请求方式也都可以通过这两种方式间接地来实现。*/

#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <mutex>
using namespace std;

#ifndef doc_root
#define doc_root "./fileroot"  //被请求的资源所在的总目录
#endif

//HTTP请求报文的请求方法：本项目只实现GET和POST
enum REQUEST_METHOD{
        GET = 1,
        POST
};
//HTTP状态码，本项目只实现6种：其语义并不严格遵照HTTP协议，且每个状态码在不同的函数里可能有多重语义
enum STATUS_CODE{
        CONTINUE               =100,    //Continue 请求解析未完
        OK                     =200,    //OK 服务器获得了完整的HTTP请求，或资源OK可以访问
        BAD_REQUEST            =400,    //Bad Request 客户端发来的HTTP请求报文有语法错误
        FORBIDDEN              =403,    //Forbidden 服务器资源禁止访问
        NOT_FOUND              =404,    //Not Found 服务器无法根据客户端的请求找到资源
        INTERNAL_SERVER_ERROR  =500     //Internal Server Error 服务器内部错误，该结果在主状态机逻辑switch的default下
};
//主状态机的状态：标识服务器解析收到来自客户端的请求报文的解析位置
enum CHECK_STATE{
        CHECK_REQUESTLINE = 1,       //正在解析请求行
        CHECK_HEADER,                //正在解析请求头
        CHECK_CONTENT                //正在解析请求体，仅用于POST请求
};
//从状态机的状态：标识解析一行的读取状态
enum LINE_STATE{
        LINE_OK = 1,                 //完整读取一行
        LINE_BAD,                    //报文有语法错误
        LINE_CONTINUE                //读取不完整
};





//该类只封装了请求报文的解析和响应报文的处理，不含客户端和服务器通信时的业务逻辑（所有的通信逻辑放在WebServer类中）
//相当于在应用层实现了简易的HTTP协议：
class Http{
public:
	Http();
    ~Http() = default;
	STATUS_CODE readRequestMessage();                 //从readBuffer读取请求报文并解析
	STATUS_CODE setResponseMessage(STATUS_CODE ret);  //根据readRequestMessage()的返回值生成响应报文

	void init();                         //重置成员，用于下次解析新的请求报文
	bool keepAlive;                      //是否是长连接，实际上现在的浏览器都是默认长连接 
	
	
    //服务器主机的若干本地信息：
	string filePath;                     // == docRoot + '/' + 除去首部action信息的url
	struct stat fileStat;                //记录文件类型、权限、size等信息
    int fileFd;                          //打开被请求的文件，即响应报文的响应体
    size_t bytes_left;                   //用于响应报文的分段传输，支持大文件传输
    
    string readBuffer;                   //暂存整个请求报文
    string writeBuffer;                  //暂存响应报文的状态行、消息头、空行
    bool reDone;                         //为true时开始发送响应报文，当（大）文件发送完毕时改成false



private:
    STATUS_CODE parseRequestLine();    //解析请求行，获得请求方法、目标URL及HTTP版本号，主状态机的状态变为CHECK_HEADER
    STATUS_CODE parseHeaders();        //解析请求头，如果是POST请求则状态转移到CHECK_CONTENT，如果是GET请求则报文解析结束。
    STATUS_CODE parseContent();        //解析请求体，仅用于POST请求

	LINE_STATE parseLine();            //从状态机简单分析每一行的语法错误，将每行数据末尾的\r\n置为\0\0
	                                   //只用于解析请求行和请求头，请求体由contentLength来界定
	STATUS_CODE doRequest();           //处理解析请求报文的结果，供readRequestMessage()调用
    

	mutex locker;                        //访问数据库时加锁。本项目中访问html文件时不必加锁，因为只读。
	unordered_map<string,string> users;  //查询或增加{user,passwd}时除了访问数据库，还要加入unordered_map
                                         //只要该连接不断，使用该连接的所有用户都能提高登录和注册速度

	
	//解析请求报文时用到的辅助变量
	size_t lineIndex;                    //主状态机的行首字符在readBuffer中的下标，string扩容时会改变存储位置，所以这里用下标而不是指针
	size_t checkedIndex;                 //从状态机读取每行字符时在readBuffer中的位置（下标）
	CHECK_STATE mainState;               //主状态
	LINE_STATE slaveState;               //从状态

	//解析请求报文得到的结果：
	REQUEST_METHOD requestMethod;        //GET或POST
	string url;                          //其前部(/, /0, /1, /2, /3, /4, /5)来自于.html文件的action属性
	string httpVersion;                  //HTTP协议版本，本项目仅支持HTTP/1.1
	string host;                         //主机名（域名），在本项目中没有实质用处
	long contentLength;                  //POST时的请求体的长度
	string requestBody;                  //请求体，用户名和密码格式：user=123&password=123

};

/*  url为请求报文中解析出的请求资源，以/开头，也就是/xxx，本项目中解析后的url有7种情况：可用于设置html文件的action属性
    /
        默认的GET请求，直接跳转到judge.html即网站首页
        其余的GET请求直接跳到url定位的页面

    /0
        POST请求，跳转到register.html，即注册页面


    /1
        POST请求，跳转到log.html，即登录页面


    /2

        POST请求，进行注册校验

        注册成功跳转到log.html，即登录页面

        注册失败跳转到registerError.html，即注册失败页面


    /3

        POST请求，进行登录校验

        验证成功跳转到welcome.html，即资源请求成功页面

        验证失败跳转到logError.html，即登录失败页面


    /4

        POST请求，跳转到picture.html，即图片请求页面


    /5

        POST请求，跳转到video.html，即视频请求页面

*/
