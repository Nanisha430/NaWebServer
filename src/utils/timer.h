#include <algorithm>
#include <functional>
#include <time.h>
#include <unordered_map>
#include <vector>

typedef std::function<void()> TimerCb;

class Timer {
  Timer() { heap_.reserve(64); }
  ~Timer() { clear(); }

  struct TimerNode {
    size_t expires;
    TimerCb cbfunc;
    size_t index;
    int id;
    bool operator<(const TimerNode &t) { return expires < t.expires; }
  };

  bool action(int id);

  bool adjust(int id, time_t newExpires);

  int add(time_t timeSlot, TimerCb cbfunc);

  bool del(int id);

  void clear();

  void tick();

private:
  void siftup(size_t i);

  void siftdown(size_t i, size_t n);

  void del(TimerNode *node);

  void action(TimerNode *node);

  int nextCount();
  int idCount_;

  std::vector<TimerNode *> heap_;
  std::unordered_map<int, TimerNode *> ref_;
};