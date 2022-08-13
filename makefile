CXX      = g++
CXXFLAGS =  -std=c++11
LIBS     = -lpthread -lmysqlclient
SOURCE   = main.cpp \
	src/base/Log.cpp     src/base/util.cpp     src/base/Socket.cpp    src/base/Epoll.cpp    \
	src/Channel.cpp      src/EventLoop.cpp     src/ThreadPool.cpp     src/MysqlConnectionPool.cpp    src/Http.cpp   src/WebServer.cpp \
	src/UserAndPassword.cpp


webserver:
	$(CXX) $(CXXFLAGS) $(SOURCE) $(LIBS) -o server 

clean:
	rm server
