#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <vector>
#include <stdarg.h> // vastart va_end
#include <assert.h>
#include "blockqueue.h"

class Log {
public:
  void init(const char *path = "./log", const char *suffix = ".log",
            int buffSize = 8192, int maxLines = 5000000,
            int maxQueueCapacity = 800);
  static Log *GetInstance() {
    static Log inst;
    return &inst;
  }

  static void FlushLogThread() { Log::GetInstance()->AsyncWrite_(); }

  void write(int level, const char *format, ...);
  void flush();

private:
  Log();
  virtual ~Log();
  void AsyncWrite_();

private:
  const static int LOG_PATH_LEN = 128;
  const static int LOG_NAME_LEN = 256;

  char path_[LOG_PATH_LEN];
  char suffix_[LOG_NAME_LEN];

  int MAX_LINE;
  int BUFF_SIZE;

  int lineCount_;
  int toDay_;

  FILE *fp_;
  char *buffer_;

  BlockDeque<std::string> *deque_;
  std::mutex mtx_;
  bool isAsync_;
  std::thread *writePID_;
};

#define LOG_BASE(level, format, ...)                                           \
  do {                                                                         \
    if (0 == this->IsCloseLog()) {                                             \
      Log::GetInstance()->write(level, format, ##__VA_ARGS__);                 \
      Log::GetInstance()->flush();                                             \
    }                                                                          \
  } while (0);

#define LOG_DEBUG(format, ...)                                                 \
  do {                                                                         \
    LOG_BASE(0, format, ..., ##__VA_ARGS__)                                    \
  } while (0);

#define LOG_INFO(format, ...)                                                  \
  do {                                                                         \
    LOG_BASE(0, format, ..., ##__VA_ARGS__)                                    \
  } while (0);
#define LOG_WARN(format, ...)                                                  \
  do {                                                                         \
    LOG_BASE(0, format, ..., ##__VA_ARGS__)                                    \
  } while (0);
#define LOG_ERROR(format, ...)                                                 \
  do {                                                                         \
    LOG_BASE(0, format, ..., ##__VA_ARGS__)                                    \
  } while (0);

#endif
