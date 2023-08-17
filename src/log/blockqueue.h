#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <sys/time.h>

template <class T> class BlockDeque {
public:
  explicit BlockDeque(size_t capacity = 2000) : capacity_(capacity) {
    if (capacity < 0) {
      exit(-1);
    }
  }
  ~BlockDeque() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      is_closed_ = true;
    }
    producer_cond_.notify_all();
    consumer_cond_.notify_all();
  };
  void clear() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      deq_.clear();
    }
  }
  bool empty() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      return deq_.empty();
    }
  }
  bool full() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      return deq_.size() >= capacity_;
    }
  }
  T front() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      return deq_.front();
    }
  }
  T back() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      return deq_.back();
    }
  }
  size_t size() {
    {
      std::lock_guard<std::mutex> locker(mtx_);
      return deq_.size();
    }
  }
  size_t capacity() { return capacity_; }
  void push_back(const T &item) {
    {
      std::unique_lock<std::mutex> locker(mtx_);
      while (deq_.size() >= capacity_) {
        producer_cond_.wait(locker);
      }
      deq_.push_back(item);
    }
    consumer_cond_.notify_one();
  }
  void push_front(const T &item) {
    {
      std::unique_lock<std::mutex> locker(mtx_);
      while (deq_.size() >= capacity_) {
        producer_cond_.wait(locker);
      }
      deq_.push_front(item);
    }
    consumer_cond_.notify_one();
  }
  bool pop(T &item) {
    {
      std::unique_lock<std::mutex> locker(mtx_);
      while (deq_.empty()) {
        consumer_cond_.wait(locker);
        if (is_closed_ == true) {
          return false;
        }
      }
      item = deq_.front();
      deq_.pop_front();
    }
    consumer_cond_.notify_one();
    return true;
  }

  bool pop(T &item, int timeout) {
    {
      std::unique_lock<std::mutex> locker(mtx_);
      while (deq_.empty()) {
        if (consumer_cond_.wait_for(locker, std::chrono::seconds(timeout)) ==
            std::cv_status::timeout) {
          item = nullptr;
          return false;
        }
        if (is_closed_ == true) {
          return false;
        }
      }
      item = deq_.front();
      deq_.pop_front();
    }
    producer_cond_.notify_one();
    return true;
  }

private:
  std::deque<T> deq_;
  std::mutex mtx_;
  size_t capacity_;
  bool is_closed_;
  std::condition_variable consumer_cond_;
  std::condition_variable producer_cond_;
};

#endif