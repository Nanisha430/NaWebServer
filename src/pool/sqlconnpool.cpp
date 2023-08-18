/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft GPL 2.0
 */

#include "sqlconnpool.h"
#include <cstddef>
#include <mutex>
#include <mysql/mysql.h>
#include <semaphore.h>

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
  for (int i = 0; i < connSize; i++) {
    MYSQL *sql = nullptr;
    mysql_init(sql);
    if (!sql) {
      LOG_ERROR("mysql init error");
      exit(1);
    }
    sql = mysql_real_connect(sql, sqlConfig_.host.c_str(),
                             sqlConfig_.user.c_str(), sqlConfig_.pwd.c_str(),
                             sqlConfig_.dbName.c_str(), sqlConfig_.port,
                             nullptr, 0);

    if (!sql) {
      LOG_ERROR("mysql connect error");
      exit(1);
    }
    connQue_.push(sql);
  }
  MAX_CONN_ = connSize;
  sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL *SqlConnPool::GetConn() {
  MYSQL *sql = nullptr;
  if (connQue_.empty()) {
    LOG_WARN("sqlConnpool busy");
    return nullptr;
  }
  {
    std::lock_guard<std::mutex> locker(mtx_);
    sql = connQue_.front();
    connQue_.pop();
  }
  return sql;
}

void SqlConnPool::FreeConn(MYSQL *conn) {
  {
    if (conn) {
      std::lock_guard<std::mutex> locker(mtx_);
      connQue_.push(conn);
      sem_post(&semId_);
    }
  }
}

void SqlConnPool::ClosePool() {
  {
    std::lock_guard<std::mutex> locker(mtx_);
    while (!connQue_.empty()) {
      auto item = connQue_.front();
      connQue_.pop();
      mysql_close(item);
    }
  }
}

int SqlConnPool::GetFreeConnCount() {
  {
    std::lock_guard<std::mutex> locker(mtx_);
    return connQue_.size();
  }
}

SqlConnPool::~SqlConnPool() { ClosePool(); }