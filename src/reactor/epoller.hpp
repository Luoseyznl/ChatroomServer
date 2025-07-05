#pragma once
#include <sys/epoll.h>

#include <map>
#include <vector>

namespace reactor {
class Epoller {
 public:
  Epoller();
  ~Epoller();

  Epoller(const Epoller&) = delete;
  Epoller& operator=(const Epoller&) = delete;

  bool addFd(int fd, uint32_t events);
  bool modFd(int fd, uint32_t events);
  bool delFd(int fd);

  int wait(std::vector<epoll_event>& activeEvents, int timeoutMs = -1);

 private:
  int epollFd_;
  std::map<int, uint32_t> fdEventsMap_;
  static constexpr int kMaxEvents = 1024;  // Maximum number of events to handle
};
}  // namespace reactor