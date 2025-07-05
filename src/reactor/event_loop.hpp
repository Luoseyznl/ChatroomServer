#pragma once
#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "channel.hpp"
#include "epoller.hpp"

class ChatroomServerEpoll;

namespace reactor {
class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  void loop();
  void quit();

  void setChatroomServerEpoll(ChatroomServerEpoll* ptr) {
    chatroomServerEpoll_ = ptr;
  }

  void addChannel(std::shared_ptr<Channel> channel);
  void updateChannel(std::shared_ptr<Channel> channel);
  void removeChannel(int fd);

 private:
  std::atomic<bool> running_{false};
  Epoller epoller_;
  std::unordered_map<int, std::shared_ptr<Channel>> channels_;
  ChatroomServerEpoll* chatroomServerEpoll_{nullptr};
};
}  // namespace reactor
