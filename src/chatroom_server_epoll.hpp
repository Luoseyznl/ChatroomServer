#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include "db/database_manager.hpp"
#include "reactor/channel.hpp"
#include "reactor/epoller.hpp"
#include "reactor/event_loop.hpp"

class ChatroomServerEpoll {
 public:
  ChatroomServerEpoll(const std::string& static_dir_path,
                      const std::string& db_file_path, int port);
  void start();
  void stop();

 private:
  void setupRoutes();
  // 静态文件处理
  // 返回内容和Content-Type
  struct StaticFileResult {
    std::string content;
    std::string contentType;
  };
  StaticFileResult handleStaticFile(const std::string& path);

  void handleNewConnection();
  void handleClientEvent(int clientFd);

  std::string staticDirPath_;
  std::shared_ptr<DatabaseManager> dbManager_;
  std::unique_ptr<reactor::EventLoop> eventLoop_;
  int listenFd_{-1};
  std::unordered_map<int, std::shared_ptr<reactor::Channel>> clientChannels_;
  std::atomic<bool> running_{false};

  // 路由表
  using Handler = std::function<std::string(
      const std::string&, const std::unordered_map<std::string, std::string>&)>;
  std::unordered_map<std::string, std::unordered_map<std::string, Handler>>
      handlers_;

  void registerHandler(const std::string& method, const std::string& path,
                       Handler handler);
  std::string dispatch(
      const std::string& method, const std::string& path,
      const std::string& body,
      const std::unordered_map<std::string, std::string>& headers);
};