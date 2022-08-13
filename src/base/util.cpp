#include "util.h"
#include "Log.h"
//#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

void errif(bool condition, const char *errmsg){
    if(condition){
        //perror(errmsg);
        LOG_ERROR(errmsg);
        exit(EXIT_FAILURE);
    }
}


void setnonblocking(int fd){
    errif(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK)==-1, "fcntl error");
}