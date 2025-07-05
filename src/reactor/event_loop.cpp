#include "event_loop.hpp"

namespace reactor {

#include <sys/epoll.h>

#include "event_loop.hpp"

EventLoop::EventLoop() : running_(false), epoller_() {}

EventLoop::~EventLoop() { quit(); }

void EventLoop::loop() {
  running_ = true;
  std::vector<epoll_event> activeEvents;
  while (running_) {
    activeEvents.clear();
    int n = epoller_.wait(activeEvents, 1000);  // 1秒超时
    for (int i = 0; i < n; ++i) {
      int fd = activeEvents[i].data.fd;
      auto it = channels_.find(fd);
      if (it != channels_.end()) {
        it->second->setRevents(activeEvents[i].events);
        it->second->handleEvent();
      }
    }
  }
}

void EventLoop::quit() {
  running_ = false;
  for (const auto& pair : channels_) {
    int fd = pair.first;
    epoller_.delFd(fd);  // 清理所有通道
  }
  channels_.clear();
}

void EventLoop::addChannel(std::shared_ptr<Channel> channel) {
  int fd = channel->getFd();
  channels_[fd] = channel;
  epoller_.addFd(fd, channel->getEvents());
}

void EventLoop::updateChannel(std::shared_ptr<Channel> channel) {
  int fd = channel->getFd();
  channels_[fd] = channel;
  epoller_.modFd(fd, channel->getEvents());
}

void EventLoop::removeChannel(int fd) {
  channels_.erase(fd);
  epoller_.delFd(fd);
}

}  // namespace reactor