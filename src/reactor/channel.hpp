#pragma once
#include <cstdint>
#include <functional>

namespace reactor {
class Channel {
 public:
  using EventCallback = std::function<void()>;

  Channel(int fd);
  ~Channel();

  int getFd() const { return fd_; };

  void setEvents(uint32_t events) { events_ = events; }
  uint32_t getEvents() const { return events_; }

  void setRevents(uint32_t revents) { revents_ = revents; }
  uint32_t revents() const { return revents_; }

  void setReadCallback(EventCallback cb) { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  void handleEvent();

 private:
  int fd_;
  uint32_t events_{0};   // 关注的事件
  uint32_t revents_{0};  // 实际发生的事件

  EventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
}  // namespace reactor