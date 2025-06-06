#include "event_loop.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "utils/logger.hpp"

namespace http {

EventLoop::EventLoop() : running_(false), epoll_fd_(-1) {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ == -1) {
    LOG_FATAL << "Failed to create epoll instance: " << strerror(errno);
    throw std::runtime_error("Failed to create epoll instance");
  }
  LOG_INFO << "EventLoop created with epoll fd " << epoll_fd_;
}

EventLoop::~EventLoop() {
  stop();

  if (epoll_fd_ != -1) {
    close(epoll_fd_);
    LOG_INFO << "Closed epoll fd " << epoll_fd_;
  }
}

void EventLoop::start() {
  if (running_) {
    LOG_WARN << "EventLoop already running";
    return;
  }

  running_ = true;
  // 启动事件循环线程
  thread_ = std::thread(&EventLoop::loop, this);
  LOG_INFO << "EventLoop started in thread " << thread_.get_id();
}

void EventLoop::stop() {
  if (!running_) {
    return;
  }

  LOG_INFO << "Stopping event loop...";
  running_ = false;

  if (thread_.joinable()) {
    thread_.join();
  }

  for (auto& pair : connections_) {
    pair.second->handleClose();
  }
  connections_.clear();

  LOG_INFO << "EventLoop stopped";
}

bool EventLoop::addConnection(std::shared_ptr<Connection> conn) {
  if (!conn) {
    LOG_ERROR << "Attempted to add null connection";
    return false;
  }

  int fd = conn->getFd();

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;  // 边缘触发，初始只监听读事件
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    LOG_ERROR << "Failed to add fd " << fd << " to epoll: " << strerror(errno);
    return false;
  }

  connections_[fd] = conn;
  LOG_DEBUG << "Added connection with fd " << fd << " to event loop";
  return true;
}

bool EventLoop::removeConnection(int fd) {
  auto it = connections_.find(fd);
  if (it == connections_.end()) {
    LOG_WARN << "Attempted to remove non-existent connection with fd " << fd;
    return false;
  }

  if (!it->second->isClosed()) {
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
      LOG_ERROR << "Failed to remove fd " << fd
                << " from epoll: " << strerror(errno);
      // 继续处理，不要返回错误
    }
  }

  it->second->handleClose();

  connections_.erase(it);
  LOG_DEBUG << "Removed connection with fd " << fd << " from event loop";
  return true;
}

bool EventLoop::modifyEvents(int fd, uint32_t events) {
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
    LOG_ERROR << "Failed to modify epoll events for fd " << fd << ": "
              << strerror(errno);
    return false;
  }

  return true;
}

void EventLoop::loop() {
  const int MAX_EVENTS = 64;
  struct epoll_event events[MAX_EVENTS];

  LOG_INFO << "Event loop running";
  last_timeout_check_ = time(nullptr);

  while (running_) {
    // 检查超时连接
    time_t now = time(nullptr);
    if (now - last_timeout_check_ >= timeout_check_interval_) {
      checkTimeouts();
      last_timeout_check_ = now;
    }

    // 等待事件，使用500ms的超时时间，以便可以定期检查连接超时
    int timeout_ms = 500;
    int n_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout_ms);

    if (n_events == -1) {
      if (errno == EINTR) {
        // 被信号中断，继续循环
        continue;
      }
      LOG_ERROR << "epoll_wait error: " << strerror(errno);
      break;
    }

    // 处理事件
    for (int i = 0; i < n_events; i++) {
      int fd = events[i].data.fd;
      uint32_t event = events[i].events;

      auto it = connections_.find(fd);
      if (it == connections_.end()) {
        LOG_ERROR << "Event for unknown fd " << fd;
        // 从epoll中移除不存在的连接
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        continue;
      }

      std::shared_ptr<Connection> conn = it->second;

      // 处理错误事件
      if (event & (EPOLLERR | EPOLLHUP)) {
        LOG_DEBUG << "Connection error or hangup on fd " << fd;
        removeConnection(fd);
        continue;
      }

      // 处理读事件
      if (event & EPOLLIN) {
        conn->handleRead();

        if (conn->isClosed()) {
          removeConnection(fd);
          continue;
        }

        // 如果有数据要写，添加写事件监听
        if (conn->hasDataToWrite()) {
          modifyEvents(fd, EPOLLIN | EPOLLOUT | EPOLLET);
        }
      }

      // 处理写事件
      if (event & EPOLLOUT) {
        conn->handleWrite();

        if (conn->isClosed()) {
          removeConnection(fd);
          continue;
        }

        // 如果没有更多数据要写，移除写事件监听
        if (!conn->hasDataToWrite()) {
          modifyEvents(fd, EPOLLIN | EPOLLET);
        }
      }
    }
  }
}

void EventLoop::checkTimeouts() {
  LOG_DEBUG << "Checking for connection timeouts";
  std::vector<int> timeouts;

  // 找出所有超时的连接
  for (const auto& pair : connections_) {
    if (pair.second->isTimedOut()) {
      timeouts.push_back(pair.first);
      LOG_INFO << "Connection with fd " << pair.first << " timed out after "
               << pair.second->getIdleTime() << "s";
    }
  }

  // 关闭超时的连接
  for (int fd : timeouts) {
    removeConnection(fd);
  }

  if (!timeouts.empty()) {
    LOG_INFO << "Closed " << timeouts.size() << " timed out connections";
  }
}

}  // namespace http