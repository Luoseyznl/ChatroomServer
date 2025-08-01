#pragma once
#include <string>

#include "db/database_manager.hpp"
#include "http/http_server.hpp"
#include "utils/kafka_producer.hpp"

class ChatroomServer {
 public:
  ChatroomServer(const std::string& static_dir_path,
                 const std::string& db_file_path, int port,
                 const std::string& kafka_brokers = "localhost:9092");
  void startServer();
  void stopServer();

 private:
  void setupRoutes();
  http::HttpResponse handleStaticFileRequest(const std::string& dir_path);

  int port_;
  std::string staticDirPath_;

  std::unique_ptr<KafkaProducer> kafkaProducer_;
  std::unique_ptr<http::HttpServer> httpServer_;
  std::shared_ptr<DatabaseManager> dbManager_;
};