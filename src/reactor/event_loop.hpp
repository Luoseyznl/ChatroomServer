#pragma once
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "channel.hpp"
#include "epoller.hpp"

namespace reactor {
class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  void addChannel(std::shared_ptr<Channel> channel);
  void updateChannel(std::shared_ptr<Channel> channel);
  void removeChannel(int fd);

 private:
  std::atomic<bool> running_{false};
  Epoller epoller_;
  std::unordered_map<int, std::shared_ptr<Channel>> channels_;
};
}  // namespace reactor
