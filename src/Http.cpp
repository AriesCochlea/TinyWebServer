#include "Http.h"
//#include "MysqlConnectionPool.h"
#include "UserAndPassword.h"
#include "base/Log.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
using namespace std;


//定义http响应报文的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file from this server.";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.";
const char *error_500_title = "Internal Server Error";
const char *error_500_form = "There was an unusual problem serving the request file.";



Http::Http():keepAlive(true), filePath(doc_root), fileFd(-1), bytes_left(0), reDone(false)
{
    init();
    /*
    //不建议每次连接时把数据库所有的（1亿？）用户名和密码都提取到unordered_map中：
    MYSQL *mysql = MysqlConnectionPool::getInstance()->getConnection();
    if (mysql_query(mysql, "SELECT username,passwd FROM user")){
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }
    MYSQL_RES *result = mysql_store_result(mysql);    //从表中检索完整的结果集
    int num_fields = mysql_num_fields(result);        //返回结果集中的列数
    MYSQL_FIELD *fields = mysql_fetch_fields(result); //返回所有字段结构的数组
    while (MYSQL_ROW row = mysql_fetch_row(result)){  //从结果集中获取下一行，将对应的用户名和密码存入unordered_map中
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
    MysqlConnectionPool::getInstance()->freeConnection(mysql); //让所有客户端竞争数据库连接池
    //数据库连接池的数量是有限的，在HTTP长连接下如果一直占用数据库连接，导致新的用户无法登录和注册
    */
}
void Http::init(){
    lineIndex = 0;
    checkedIndex =0;
    mainState = CHECK_REQUESTLINE;
    slaveState = LINE_CONTINUE;
    requestMethod = GET;
    url = "";
    httpVersion = "HTTP/1.1";
    host = "";
    contentLength = 0;
    requestBody = "";
}




//解析请求行和请求头时仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。
//解析请求体时，请求体的末尾可能没有任何字符，不能使用从状态机的状态，改用主状态机的状态作为循环入口条件。
STATUS_CODE Http::readRequestMessage(){
    while( ((slaveState = parseLine()) == LINE_OK ) || (mainState==CHECK_CONTENT) ){
        //LOG_INFO(readBuffer.c_str() + lineIndex); //把请求报文的每行（不论对错）都记在日志文件上便于调试
        STATUS_CODE ret;
        switch (mainState){                    //主状态机
            case CHECK_REQUESTLINE:
                ret = parseRequestLine();
                switch (ret){                  //从状态机
                case CONTINUE://mainState由CHECK_REQUESTLINE变成CHECK_HEADER
                    lineIndex = checkedIndex;  //指向下一行
                    //LOG_INFO("readRequestMessage CONTINUE");
                    break;
                case BAD_REQUEST:
                    //LOG_INFO("readRequestMessage BAD_REQUEST");
                    return BAD_REQUEST;
                default:
                    break;
                }
                break;
            case CHECK_HEADER:
                ret = parseHeaders();
                switch (ret){                  //从状态机
                case OK:
                    return doRequest();//OK;NOT_FOUND;FORBIDDEN;BAD_REQUEST;INTERNAL_SERVER_ERROR
                    break;
                case CONTINUE://如果是POST请求，则mainState由CHECK_HEADER变成CHECK_CONTENT
                    lineIndex = checkedIndex;  //指向下一行
                default:
                    break;
                }
                break;
            case CHECK_CONTENT:
                ret = parseContent();
                switch (ret){                  //从状态机
                case OK:
                    return doRequest();//OK;NOT_FOUND;FORBIDDEN;BAD_REQUEST;INTERNAL_SERVER_ERROR
                    break;
                case CONTINUE:
                    return CONTINUE;
                    break;
                default:
                    break;
                }
                break;
            default:
                return INTERNAL_SERVER_ERROR;
                break;
        }
    }
    if(mainState!=CHECK_CONTENT){
        if(slaveState == LINE_BAD)
            return BAD_REQUEST;
        if(slaveState == LINE_CONTINUE)
            return CONTINUE;
    }
    return INTERNAL_SERVER_ERROR;
} //返回值为以下6种之一：CONTINUE、OK、BAD_REQUEST、INTERNAL_SERVER_ERROR、NOT_FOUND、FORBIDDEN



STATUS_CODE Http:: setResponseMessage(STATUS_CODE ret){
    char temp[128];
    switch (ret){
    case CONTINUE:       //解析请求未完，可能是请求文件太大，客户端要分几次发送
        return CONTINUE; //不重置，保存现场
        break;    
    case INTERNAL_SERVER_ERROR:
        sprintf(temp, "%s %d %s\r\n", "HTTP/1.1", 500, error_500_title); //响应报文的状态行
        writeBuffer = temp;
        sprintf(temp, "Content-Length:%lu\r\n", strlen(error_500_form));
        writeBuffer += temp;
        sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
        writeBuffer += temp;
        writeBuffer += "\r\n";                                           //空行作为响应头结束的标志
        writeBuffer += error_500_form;//响应体为"There was an unusual problem serving the request file.\n"
        init();                       //重置
        bytes_left = writeBuffer.size();
        LOG_INFO(error_500_form);
        return INTERNAL_SERVER_ERROR;
        break;
    case BAD_REQUEST:
        sprintf(temp, "%s %d %s\r\n", "HTTP/1.1", 400, error_400_title);
        writeBuffer = temp;
        sprintf(temp, "Content-Length:%lu\r\n", strlen(error_400_form));
        writeBuffer += temp;
        sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
        writeBuffer += temp;
        writeBuffer += "\r\n";  
        writeBuffer += error_400_form;
        init();
        bytes_left = writeBuffer.size();
        LOG_INFO(error_400_form);
        return BAD_REQUEST;
        break;
    case NOT_FOUND:
        sprintf(temp, "%s %d %s\r\n", "HTTP/1.1", 404, error_404_title);
        writeBuffer = temp;
        sprintf(temp, "Content-Length:%lu\r\n", strlen(error_404_form));
        writeBuffer += temp;
        sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
        writeBuffer += temp;
        writeBuffer += "\r\n";  
        writeBuffer += error_404_form;
        init();
        bytes_left = writeBuffer.size();
        LOG_INFO(error_404_form);
        return NOT_FOUND;
        break;
    case FORBIDDEN:
        sprintf(temp, "%s %d %s\r\n", "HTTP/1.1", 403, error_403_title);
        writeBuffer = temp;
        sprintf(temp, "Content-Length:%lu\r\n", strlen(error_403_form));
        writeBuffer += temp;
        sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
        writeBuffer += temp;
        writeBuffer += "\r\n";  
        writeBuffer += error_403_form;
        init();
        bytes_left = writeBuffer.size();
        LOG_INFO(error_403_form);
        return FORBIDDEN;
        break;
    case OK:
        sprintf(temp, "%s %d %s\r\n", "HTTP/1.1", 200, ok_200_title);
        writeBuffer = temp;
        if(fileStat.st_size == 0){ //如果请求的资源大小为0，则返回空白html文件
            const char* ok_string="<html><body></body></html>";
            sprintf(temp, "Content-Length:%lu\r\n", strlen(ok_string));
            writeBuffer +=temp;
            sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
            writeBuffer += temp;
            writeBuffer += "\r\n"; 
            writeBuffer += ok_string;
            init();
            bytes_left = writeBuffer.size();
        }
        else{
            sprintf(temp, "Content-Length:%ld\r\n", fileStat.st_size); //客户端会根据Content-Length判断服务器的文件是否发送完
            writeBuffer +=temp;
            sprintf(temp, "Connection:%s\r\n", (keepAlive == true) ? "keep-alive" : "close");
            writeBuffer += temp;
            writeBuffer += "\r\n"; 
            init();
            bytes_left = writeBuffer.size() + fileStat.st_size;
        }
        return OK;
        break;
    default: 
        init();
        bytes_left = 0;
        return INTERNAL_SERVER_ERROR;
        break;
    }

} 





STATUS_CODE Http::parseRequestLine(){
    const char* lineText = readBuffer.c_str() + lineIndex;
    LOG_INFO(lineText);                     //记录每个请求报文的请求行
    const char *p1 = strpbrk(lineText," \t"); 
    if(p1==nullptr){
        //LOG_INFO("bad request method");
        return BAD_REQUEST;
    }
    readBuffer[p1-lineText]='\0';          //将该位置改为\0，用于将前面数据取出
    ++p1;
    if(strcasecmp(lineText,"GET")==0){     //strcasecmp比较时忽略大小写
        requestMethod = GET;
    }
    else if(strcasecmp(lineText,"POST")==0){
        requestMethod = POST;
    }
    else {
        //LOG_INFO("bad request method");
        return BAD_REQUEST;
    }

    p1 += strspn(p1," \t");
    const char *p2 = strpbrk(p1," \t");
    if(p2==nullptr){
        //LOG_INFO("bad url");
        return BAD_REQUEST;
    }
    readBuffer[p2-lineText]='\0'; 
    ++ p2;
    p1 = strrchr(p1,'/');
    if(p1==nullptr){
        //LOG_INFO("bad url");
        return BAD_REQUEST;
    }
    url = p1;                           //url的格式为"/资源名"：最后一个'/'前面的内容被忽略，资源名可为空


    p2 +=strspn(p2, " \t");             
    if(strcasecmp(p2,"HTTP/1.1")!=0 && strcasecmp(p2,"HTTP1.1")!=0){  //仅支持HTTP/1.1
        //LOG_INFO("bad HTTP vertion");
        return BAD_REQUEST;
    }
    else{
        httpVersion = p2;              //行末的\r\n已经被parseLine()换成'\0'了
    }
    mainState = CHECK_HEADER;          //写在readRequestMessage()函数中可读性更高
    return CONTINUE;
}





STATUS_CODE Http:: parseHeaders(){
    const char* lineText = readBuffer.c_str() + lineIndex;//即使请求头有多行，每次调用parseHeaders也只处理一行
    const char* p= lineText;
    if(*lineText == '\0'){            //空行是请求头结束的标志
        if(requestMethod == POST){
            mainState = CHECK_CONTENT;//POST时需要解析请求体，写在readRequestMessage()函数中可读性更高
            return CONTINUE;
        }
        else
            return OK;
    }
    else if(strncasecmp(p, "Host:", 5)==0){
        p += 5;
        p +=strspn(p, " \t");
        host = p;
    }
    else if(strncasecmp(p, "Connection:", 11)==0){
        p += 11;
        p +=strspn(p, " \t");
        if(strcasecmp(p,"Keep-Alive")==0){
            keepAlive = true;
        }
    }
    else if(strncasecmp(p, "Content-Length:", 15)==0){
        p += 15;
        p +=strspn(p, " \t");
        contentLength = atol(p);
    }
    else{ //本项目只实现上述3种请求头选项，即：Host、Connection、Content-Length
       //LOG_INFO("unknow HTTP header: %s", lineText); //而不是return BAD_REQUEST;
    }
    return CONTINUE; //继续读请求头的下一个选项
} 



//并没有真正解析HTTP请求的消息体，只是判断它是否完整地读入了
STATUS_CODE Http:: parseContent(){
    if(lineIndex + contentLength <= readBuffer.size()){ //判断消息体是否完整
        requestBody.assign(readBuffer,lineIndex, contentLength);
        return OK;
    } 
    else
        return CONTINUE;
} 




LINE_STATE Http:: parseLine(){
    for(size_t size=readBuffer.size();checkedIndex<size; ++checkedIndex){
        if(readBuffer[checkedIndex]=='\r'){
            if(checkedIndex +1 == size)
                return LINE_CONTINUE;
            else if(readBuffer[checkedIndex+1]=='\n'){
                readBuffer[checkedIndex++]='\0';
                readBuffer[checkedIndex++]='\0';  //checkedIndex指向下一行
                return LINE_OK;
            }
            else
                return LINE_BAD;
        }
        //一般是上次读取到\r就到readBuffer末尾了，没有接收完整，再次接收时会出现这种情况
        else if(readBuffer[checkedIndex]=='\n'){
            if(checkedIndex > 0 && readBuffer[checkedIndex -1] == '\r'){
                readBuffer[checkedIndex-1] ='\0';
                readBuffer[checkedIndex++]='\0'; //checkedIndex指向下一行
                return LINE_OK;  
            }
            else 
                return LINE_BAD;
        }
    }
    return LINE_CONTINUE; //没有找到\r\n，需要继续
}






STATUS_CODE  Http::doRequest(){
	if(bytes_left > 0)
		return FORBIDDEN;         //为了防止filePath、fileStat、fileFd在解析新请求报文时被修改
		
    const char *p = url.c_str();
    ++ p;
    filePath = doc_root;
    if(requestMethod == GET){
        if(*p == '\0'){
            filePath.append("/judge.html"); 
        }
        else{
            filePath = doc_root + url;
        }
    }
    else if(requestMethod == POST){
        if(*p == '0'){
            filePath.append("/register.html");
        }
        else if(*p == '1'){
            filePath.append("/log.html");
        }
        else if(*p == '2'){
        //将用户名和密码提取出来
        //user=123&password=123
            char name[100], password[100];
            int i, j;
            for(i=5; requestBody[i]!='&'; ++i)
                name[i-5] = requestBody[i];
            name[i-5] = '\0';
            i += 10;//跳过“&password=”
            for(j=0; requestBody[i]!='\0';++i, ++j)
                password[j] = requestBody[i];
            password[j] = '\0';
            //注册校验：先检测数据库中是否有重名的，没有重名的就增加数据
            if(users.find(name)==users.end()){
            /*    char sql_insert[256];
                sprintf(sql_insert, "INSERT INTO user(username, passwd) VALUES('%s', '%s')",name,password);
                MYSQL *mysql = MysqlConnectionPool::getInstance()->getConnection();
                locker.lock();
                int ret = mysql_query(mysql, sql_insert); //如果查询成功，返回0。如果出现错误，返回非0值。
                locker.unlock();
                MysqlConnectionPool::getInstance()->freeConnection(mysql);
                if(ret == 0){
                    filePath.append("/log.html");         //注册成功跳转到log.html，即登录页面
                }
                else {
                    filePath.append("/registerError.html");
                }
                users[name] = password;                   //除了加入数据库，还要加入unordered_map提高访问速度
            */
                UserAndPassword* userAndPassword = UserAndPassword::getInstance();
                int ret = userAndPassword->readUser(name,password);
                if(ret==0){                                      //用户名不存在，开始注册
                    userAndPassword->addUser(name,password);
                    users[name] = password;
                    filePath.append("/log.html");                //注册成功跳转登录页面
                }
                else if(ret==1){                                 //用户名和密码已存在，直接登录
                    users[name] = password;
                    filePath.append("/log.html");                //注册成功跳转登录页面
                }
                else{                                            //用户名存在，但密码错误，禁止注册
                    filePath.append("/registerError.html");
                }
            }
            else if(users[name] == password){
                filePath.append("/log.html");             //用户名和密码已存在，无需注册，直接登录
            }
            else{
                filePath.append("/registerError.html");   //用户名已存在，但密码不对（用户名已被别人注册）
            }
        }
        else if(*p == '3'){
            char name[128], password[128];
            int i, j;
            for(i=5; requestBody[i]!='&'; ++i)
                name[i-5] = requestBody[i];
            name[i-5] = '\0';
            i += 10;
            for(j=0; requestBody[i]!='\0';++i, ++j)
                password[j] = requestBody[i];
            password[j] = '\0';
            //登录检验：浏览器端输入的用户名和密码在unordered_map中可以查找到则返回1，否则返回0
            if(users.find(name) != users.end() && users[name] == password)
                filePath.append("/welcome.html");
            else if( UserAndPassword::getInstance()->readUser(name,password)==1){
                users[name] = password;
                filePath.append("/welcome.html");
            }
            else 
                filePath.append("/logError.html");        //用户名未注册或密码错误
        }
        else if(*p == '4'){
            filePath.append("/picture.html");
        }
        else if(*p == '5'){
            filePath.append("/video.html");
        }
        else{  
            filePath = doc_root + url;                    //如果以上均不符合，直接将url与网站目录拼接
        }
    }


    
    if(stat(filePath.c_str(), &fileStat) < 0)
        return NOT_FOUND;                      //资源不存在
    if(!(fileStat.st_mode & S_IROTH))          //判断文件的权限，是否可读，不可读则返回FORBIDDEN
        return FORBIDDEN;
    if(S_ISDIR (fileStat.st_mode))             //判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
        return BAD_REQUEST;
    fileFd = open(filePath.c_str(), O_RDONLY); //以“只读”方式打开
    if(fileFd == -1)
        return INTERNAL_SERVER_ERROR;
    return OK;


}
