#include "epoller.hpp"

#include <unistd.h>

#include <cstring>

#include "utils/logger.hpp"

namespace reactor {

Epoller::Epoller() {
  epollFd_ = epoll_create1(EPOLL_CLOEXEC);
  if (epollFd_ < 0) {
    LOG(ERROR) << "Failed to create epoll instance: " << strerror(errno);
    throw std::runtime_error("Failed to create epoll instance");
  }
}

Epoller::~Epoller() {
  if (epollFd_ >= 0) {
    close(epollFd_);
  }
}

bool Epoller::addFd(int fd, uint32_t events) {
  epoll_event ev;
  std::memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
    fdEventsMap_[fd] = events;
    return true;
  }
  return false;
}

bool Epoller::modFd(int fd, uint32_t events) {
  epoll_event ev;
  std::memset(&ev, 0, sizeof(ev));
  ev.events = events;
  ev.data.fd = fd;
  if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) == 0) {
    fdEventsMap_[fd] = events;
    return true;
  }
  return false;
}

bool Epoller::delFd(int fd) {
  epoll_event ev;
  std::memset(&ev, 0, sizeof(ev));
  ev.data.fd = fd;
  if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev) == 0) {
    fdEventsMap_.erase(fd);
    return true;
  }
  return false;
}

int Epoller::wait(std::vector<epoll_event>& activeEvents, int timeoutMs) {
  epoll_event events[kMaxEvents];
  int n = epoll_wait(epollFd_, events, kMaxEvents, timeoutMs);
  if (n > 0) {
    activeEvents.assign(events, events + n);
  }
  return n;
}

}  // namespace reactor