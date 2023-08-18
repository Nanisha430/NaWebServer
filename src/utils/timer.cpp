#include "timer.h"
#include <cassert>

// 基于小根堆实现的定时器，关闭超时的非活动连接；
void Timer::siftup(size_t i) {
  int j = (i - 1) / 2;
  while (i > 0 && heap_[i] < heap_[j]) {
    std::swap(heap_[i], heap_[j]);
    heap_[i]->index = i;
    heap_[j]->index = j;
    i = j;
    j = (i - 1) / 2;
  }
}

void Timer::siftdown(size_t i, size_t n) {
  // n 为最后一个结点  j为左孩子 i从0开始
  size_t j = 2 * i + 1;
  while (j < n) {
    if (j + 1 < n && heap_[j + 1] < heap_[j])
      j++;

    if (heap_[i] < heap_[j])
      break;
    std::swap(heap_[i], heap_[j]);
    heap_[i]->index = i;
    heap_[j]->index = j;
    i = j;
    j = i * 2 + 1;
  }
}

int Timer::add(time_t timeSlot, TimerCb cbfunc) {
  time_t now = time(nullptr);
  size_t expires = now + timeSlot;
  size_t index = heap_.size();
  int id = Timer::nextCount();
  TimerNode *timenode = new TimerNode({expires, cbfunc, index, id});
  heap_.push_back(timenode);
  siftup(heap_.size() - 1);
  ref_[id] = timenode;
  return id;
}

bool Timer::del(int id) {
  if (ref_.count(id) != 1) {
    return false;
  }
  del(ref_[id]);
  return true;
}

void Timer::del(TimerNode *node) {
  assert(node != nullptr && !heap_.empty());
  size_t i = node->index;
  size_t n = heap_.size() - 1;
  assert(i <= n);
  if (i != n) {
    std::swap(heap_[i], heap_[n]);
    heap_[i]->index = i;
    siftdown(i, n);
    siftup(i);
  }
  heap_.pop_back();
  ref_.erase(node->id);
  delete node;
}

bool Timer::action(int id) {
  if (ref_.count(id) != 1)
    return false;
  auto cb = std::move(heap_[id]->cbfunc);
  if (cb) {
    cb();
  }
  return del(id);
}

void Timer::action(TimerNode *node) {
  auto cb = node->cbfunc;
  if (cb)
    cb();
  del(node);
}

bool Timer::adjust(int id, time_t newExpires) {
  if (ref_.count(id) != 1)
    return false;
  ref_[id]->expires = newExpires;
  siftdown(ref_[id]->index, heap_.size());
  return true;
}

void Timer::tick() {
  if (heap_.empty()) {
    return;
  }
  time_t now = time(nullptr);
  while (!heap_.empty()) {
    auto node = heap_.front();
    if (now < node->expires) {
      break;
    }
    action(node);
  }
}

int Timer::nextCount() { return ++idCount_; }

void Timer::clear() {
  ref_.clear();
  for (auto &item : heap_) {
    delete item;
  }
  heap_.clear();
}