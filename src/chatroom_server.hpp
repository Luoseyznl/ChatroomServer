#pragma once
#include <string>

#include "db/database_manager.hpp"
#include "http/http_server.hpp"

class ChatroomServer {
 public:
  ChatroomServer(const std::string& static_dir_path,
                 const std::string& db_file_path);
  void startServer(int port);
  void stopServer();

 private:
  void setupRoutes();
  http::HttpResponse handleStaticFileRequest(const std::string& dir_path);

  std::string staticDirPath_;

  std::unique_ptr<http::HttpServer> httpServer_;
  std::shared_ptr<DatabaseManager> dbManager_;
};