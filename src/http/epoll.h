#include <fcntl.h> // fcntl()
#include <sys/epoll.h>
#include <unistd.h> // close()
class Epoll {
public:
  Epoll(int eventsize = 10000, int timeout = -1);
  ~Epoll();
  int Wait();
  // oneshot 一个线程处理一个socket链接
  void AddFd(int fd, bool enableET = true, bool enableOneShot = false);

  void RemoveFd(int fd);
  // ET 边缘触发 一次必须处理完所有的数据
  void Modify(int fd, uint32_t mode, bool enableET = true,
              bool enableOneShot = true);

  int SetNonblock(int fd);

  int GetFd() const;

  int GetEventFd(int i) const;

  uint32_t GetEvent(int i) const;

private:
  int eventSize_;
  int timeout_;
  int epollFd_;

  epoll_event *ev_;
};