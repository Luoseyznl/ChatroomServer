#include "channel.hpp"

#include <sys/epoll.h>
#include <unistd.h>

namespace reactor {
Channel::Channel(int fd) : fd_(fd) {}

Channel::~Channel() {
  // 不负责关闭fd，由上层（如EventLoop）管理
}

void Channel::handleEvent() {
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) closeCallback_();
    return;
  }
  if (revents_ & EPOLLERR) {
    if (errorCallback_) errorCallback_();
    return;
  }
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) readCallback_();
  }
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) writeCallback_();
  }
}
}  // namespace reactor