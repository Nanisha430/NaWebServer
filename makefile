#heaptimer.cpp 
CXX = g++
CFLAGS = -std=c++14 -g 

TARGET = server
OBJS = ./src/main.cpp ./src/log/log.cpp ./src/pool/sqlconnpool.cpp ./src/http/httpconn.cpp ./src/timer/heaptimer.cpp ./src/webserver.cpp 

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf $(OBJS) $(TARGET)