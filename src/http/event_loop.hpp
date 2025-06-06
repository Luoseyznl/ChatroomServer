#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include "connection.hpp"

namespace http {

/**
 * @brief 事件循环类，使用epoll管理连接
 *
 * 负责：
 * 1. 管理所有客户端连接
 * 2. 监听读写事件
 * 3. 分发事件到对应的连接处理
 * 4. 管理连接超时
 */
class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  void start();
  void stop();

  bool addConnection(std::shared_ptr<Connection> conn);
  bool removeConnection(int fd);

  void setTimeoutCheckInterval(int seconds) {
    timeout_check_interval_ = seconds;
  }

 private:
  void loop();

  bool modifyEvents(int fd, uint32_t events);

  void checkTimeouts();

  int epoll_fd_;
  std::atomic<bool> running_;
  std::thread thread_;

  // 连接管理
  std::map<int, std::shared_ptr<Connection>> connections_;

  // 超时管理
  time_t last_timeout_check_ = 0;    // 上次检查超时的时间
  int timeout_check_interval_ = 10;  // 超时检查间隔（秒）
};

}  // namespace http