#pragma once

#include <memory>
#include <string>

#include "db/database_manager.hpp"
#include "http/http_server.hpp"
#include "http/reactor_server.hpp"

/**
 * @brief 聊天应用程序主类
 *
 * 功能：用户注册登录、聊天室创建与加入、消息发送与接收、用户状态管理等。
 */
class ChatApplication {
 public:
  ChatApplication(const std::string& static_dir);

  void start(int port);
  void stop();

 private:
  void setupRoutes();

  http::HttpResponse serveStaticFile(const std::string& path);

  std::string static_dir_;
  std::unique_ptr<http::ReactorServer> http_server_;
  std::shared_ptr<DatabaseManager> db_manager_;
};
