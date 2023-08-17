#include "sqlconnpool.h"

SqlConnPool::SqlConnPool() {
  useCount_ = 0;
  freeCount_ = 0;
}

SqlConnPool *SqlConnPool::GetInstance() {
  static SqlConnPool connPool;
  return &connPool;
}
void SqlConnPool::Init(std::string host, std::string user, std::string pwd,
                       std::string dbName, int port, int connSize,
                       bool isCloseLog) {
  sqlConfig_ = {host, user, pwd, dbName, port};
  isCloseLog_ = isCloseLog;
}
