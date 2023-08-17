#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include "../log/log.h"
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <semaphore.h>
#include <string>
#include <thread>

class SqlConnPool {
public:
  static SqlConnPool *GetInstance();

  MYSQL *GetConn();
  void FreeConn(MYSQL *conn);
  int GetFreeConnCount();

  void Init(std::string host, std::string user, std::string pwd,
            std::string dbName, int port, int connSize,
            bool isCloseLog = false);

  void ClosePool();
  bool IsCloseLog() { return isCloseLog_; };

private:
  SqlConnPool();
  ~SqlConnPool();

  struct sqlConfig {
    std::string host;
    std::string user;
    std::string pwd;
    std::string dbName;
    int port;
  } sqlConfig_;

  bool isCloseLog_;
  int MAX_CONN_;
  int useCount_;
  int freeCount_;

  std::queue<MYSQL *> connQue_;
  std::mutex mtx_;
  sem_t semId_;
};

#endif