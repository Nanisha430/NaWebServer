#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

class ThreadPool {
  explicit ThreadPool(size_t threadNum ) : pool_(std::make_shared<Pool>()) {

    for (size_t i = 0; i < threadNum; i++) {
      std::thread([pool = pool_] {
        std::unique_lock<std::mutex> locker(pool->mtx);
        while (true) {
          if (!pool->tasks.empty()) {
            auto current = std::move(pool->tasks.front());
            pool->tasks.pop();
            locker.unlock();
            current();
            locker.lock();
          } else if (pool->isClosed) {
            break;
          } else {
            // 当线程池任务为空时，阻塞
            pool->cond.wait(locker);
          }
        }
      }).detach(); // detach 不阻塞主线程运行
    }
  }
  ThreadPool() = default;
  ThreadPool(ThreadPool &&) = default;
  ~ThreadPool() {
    if (static_cast<bool>(pool_)) {
      {
        std::lock_guard<std::mutex> locker(pool_->mtx);
        pool_->isClosed = true;
      }
      pool_->cond.notify_all();
    }
  }
  template <class F> void addTask(F &&task) {
    {
      std::lock_guard<std::mutex> locker(pool_->mtx);
      pool_->tasks.emplace(task);
    }
    pool_->cond.notify_one();
  }

private:
  struct Pool {
    std::mutex mtx;
    std::condition_variable cond;
    bool isClosed;
    std::queue<std::function<void()>> tasks;
  };
  std::shared_ptr<Pool> pool_;
};

#endif