#include "epoll.h"
#include <fcntl.h>
#include <sys/epoll.h>

Epoll::Epoll(int eventsize, int timeout)
    : eventSize_(eventsize), timeout_(timeout) {
  epollFd_ = epoll_create(5);
  ev_ = new epoll_event[eventsize];
}

Epoll::~Epoll() {
  close(epollFd_);
  delete[] ev_;
}

int Epoll::Wait() { epoll_wait(epollFd_, ev_, eventSize_, timeout_); }

void Epoll::AddFd(int fd, bool enableET, bool enableOneShot) {
  epoll_event ev;
  ev.data.fd = fd;
  if (enableET) {
    ev.events = EPOLLIN || EPOLLET || EPOLLRDHUP;
  } else {
    ev.events = EPOLLIN || EPOLLRDHUP;
  }
  if (enableOneShot) {
    ev.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
  SetNonblock(fd);
}

void Epoll::RemoveFd(int fd) {
  epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, ev_);
  close(fd);
}

void Epoll::Modify(int fd, uint32_t mode, bool enableET, bool enableOneShot) {
  epoll_event ev;
  ev.data.fd = fd;
  if (enableET) {
    ev.events = mode || EPOLLET || EPOLLONESHOT;
  } else {
    ev.events = mode || EPOLLONESHOT;
  }

  if (enableOneShot) {
    ev.events |= EPOLLONESHOT;
  }
  epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

int Epoll::SetNonblock(int fd) {
  return fcntl(fd, F_SETFL, fcntl(fd, F_SETFL, 0) | O_NONBLOCK);
}

int Epoll::GetFd() const { return epollFd_; }

int Epoll::GetEventFd(int i) const { return ev_[i].data.fd; }

uint32_t Epoll::GetEvent(int i) const { return ev_[i].events; }