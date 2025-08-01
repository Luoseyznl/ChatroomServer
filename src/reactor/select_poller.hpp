#pragma once

#include <map>
#include <vector>
#include <sys/select.h>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <iostream>

namespace reactor {

struct SelectEvent {
    int fd;
    uint32_t events; // 1: read, 2: write, 4: except
};

class SelectPoller {
 public:
  SelectPoller() = default;
  ~SelectPoller() = default;

  SelectPoller(const SelectPoller&) = delete;
  SelectPoller& operator=(const SelectPoller&) = delete;

  // events: 1=read, 2=write, 4=except
  bool addFd(int fd, uint32_t events) {
    if (fd >= FD_SETSIZE) {
      std::cerr << "[SelectPoller] fd " << fd << " >= FD_SETSIZE(" << FD_SETSIZE << "), addFd failed!" << std::endl;
      return false;
    }
    fdEventsMap_[fd] = events;
    return true;
  }

  bool modFd(int fd, uint32_t events) {
    if (fdEventsMap_.count(fd) == 0) return false;
    if (fd >= FD_SETSIZE) {
      std::cerr << "[SelectPoller] fd " << fd << " >= FD_SETSIZE(" << FD_SETSIZE << "), modFd failed!" << std::endl;
      return false;
    }
    fdEventsMap_[fd] = events;
    return true;
  }

  bool delFd(int fd) {
    return fdEventsMap_.erase(fd) > 0;
  }

  int wait(std::vector<SelectEvent>& activeEvents, int timeoutMs = -1) {
    fd_set readfds, writefds, exceptfds;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);

    int maxfd = -1;
    for (const auto& [fd, events] : fdEventsMap_) {
      if (fd >= FD_SETSIZE) {
        std::cerr << "[SelectPoller] fd " << fd << " >= FD_SETSIZE(" << FD_SETSIZE << "), skip in wait!" << std::endl;
        continue;
      }
      if (events & 1) FD_SET(fd, &readfds);
      if (events & 2) FD_SET(fd, &writefds);
      if (events & 4) FD_SET(fd, &exceptfds);
      if (fd > maxfd) maxfd = fd;
    }

    struct timeval tv, *ptv = nullptr;
    if (timeoutMs >= 0) {
      tv.tv_sec = timeoutMs / 1000;
      tv.tv_usec = (timeoutMs % 1000) * 1000;
      ptv = &tv;
    }

    int n = select(maxfd + 1, &readfds, &writefds, &exceptfds, ptv);
    if (n > 0) {
      for (const auto& [fd, events] : fdEventsMap_) {
        if (fd >= FD_SETSIZE) continue;
        uint32_t revents = 0;
        if ((events & 1) && FD_ISSET(fd, &readfds)) revents |= 1;
        if ((events & 2) && FD_ISSET(fd, &writefds)) revents |= 2;
        if ((events & 4) && FD_ISSET(fd, &exceptfds)) revents |= 4;
        if (revents) activeEvents.push_back({fd, revents});
      }
    }
    return n;
  }

 private:
  std::map<int, uint32_t> fdEventsMap_;
};

}  // namespace reactor