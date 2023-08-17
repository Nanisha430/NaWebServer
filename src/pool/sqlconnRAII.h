#include "sqlconnpool.h"

class SqlConnRAII {
public:
  SqlConnRAII(MYSQL **sql, SqlConnPool *connpool) {
    *sql = connpool->GetConn();
    sql_ = *sql;
    connpool_ = connpool;
  }

  ~SqlConnRAII() {
    if (sql_) {
      connpool_->FreeConn(sql_);
    }
    connpool_->ClosePool();
  }

private:
  MYSQL *sql_;
  SqlConnPool *connpool_;
};