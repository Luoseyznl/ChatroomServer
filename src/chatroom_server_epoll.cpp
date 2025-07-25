#include "chatroom_server_epoll.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "utils/logger.hpp"

namespace {

int setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool ends_with(const std::string& value, const std::string& ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

std::string getContentType(const std::string& path) {
  if (ends_with(path, ".html") || ends_with(path, ".htm")) return "text/html";
  if (ends_with(path, ".css")) return "text/css";
  if (ends_with(path, ".js")) return "application/javascript";
  if (ends_with(path, ".json")) return "application/json";
  if (ends_with(path, ".png")) return "image/png";
  if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return "image/jpeg";
  if (ends_with(path, ".ico")) return "image/x-icon";
  return "text/plain";
}

// 简单HTTP请求解析（仅支持单次收包，生产环境需完善）
bool parseHttpRequest(const std::string& raw, std::string& method,
                      std::string& path,
                      std::unordered_map<std::string, std::string>& headers,
                      std::string& body) {
  size_t pos = raw.find("\r\n\r\n");
  if (pos == std::string::npos) return false;
  std::istringstream iss(raw.substr(0, pos));
  std::string line;
  if (!std::getline(iss, line)) return false;
  std::istringstream reqline(line);
  reqline >> method >> path;
  while (std::getline(iss, line)) {
    if (line.empty() || line == "\r") break;
    size_t sep = line.find(':');
    if (sep != std::string::npos) {
      std::string key = line.substr(0, sep);
      std::string value = line.substr(sep + 1);
      while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
        value.erase(0, 1);
      if (!value.empty() && value.back() == '\r') value.pop_back();
      headers[key] = value;
    }
  }
  body = raw.substr(pos + 4);
  return true;
}

std::string makeHttpResponse(
    const std::string& body,
    const std::string& contentType = "application/json", int status = 200) {
  std::ostringstream oss;
  oss << "HTTP/1.1 " << status << " OK\r\n";
  oss << "Content-Type: " << contentType << "\r\n";
  oss << "Content-Length: " << body.size() << "\r\n";
  oss << "Connection: close\r\n\r\n";
  oss << body;
  return oss.str();
}

}  // namespace

ChatroomServerEpoll::ChatroomServerEpoll(const std::string& static_dir_path,
                                         const std::string& db_file_path,
                                         int port,
                                         const std::string& kafka_brokers)
    : staticDirPath_(static_dir_path),
      dbManager_(std::make_shared<DatabaseManager>(db_file_path)),
      kafkaProducer_(
          std::make_unique<KafkaProducer>(kafka_brokers, "chatroom_events")),
      eventLoop_(std::make_unique<reactor::EventLoop>()),
      listenFd_(-1),
      running_(false) {
  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) throw std::runtime_error("Failed to create listen socket");
  setNonBlocking(listenFd_);
  int opt = 1;
  setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    close(listenFd_);
    throw std::runtime_error("Failed to bind listen socket");
  }
  if (listen(listenFd_, SOMAXCONN) < 0) {
    close(listenFd_);
    throw std::runtime_error("Failed to listen");
  }
  setupRoutes();
  eventLoop_->setChatroomServerEpoll(this);
  LOG(INFO) << "Chatroom server initialized on port " << port;
}

void ChatroomServerEpoll::start() {
  running_ = true;
  auto listenChannel = std::make_shared<reactor::Channel>(listenFd_);
  listenChannel->setEvents(EPOLLIN);
  listenChannel->setReadCallback([this]() { handleNewConnection(); });
  eventLoop_->addChannel(listenChannel);
  eventLoop_->loop();
}

void ChatroomServerEpoll::stop() {
  running_ = false;
  if (listenFd_ != -1) {
    close(listenFd_);
    listenFd_ = -1;
  }
  eventLoop_->quit();
  LOG(INFO) << "Chatroom server stopped";
}

void ChatroomServerEpoll::handleNewConnection() {
  while (true) {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int clientFd = accept(listenFd_, (sockaddr*)&client_addr, &client_len);
    if (clientFd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else {
        std::cerr << "accept error: " << strerror(errno) << std::endl;
        break;
      }
    }
    setNonBlocking(clientFd);
    auto clientChannel = std::make_shared<reactor::Channel>(clientFd);
    clientChannel->setEvents(EPOLLIN | EPOLLET);
    clientChannel->setReadCallback(
        [this, clientFd]() { handleClientEvent(clientFd); });
    clientChannels_[clientFd] = clientChannel;
    eventLoop_->addChannel(clientChannel);
    LOG(INFO) << "New client connected: " << inet_ntoa(client_addr.sin_addr)
              << ":" << ntohs(client_addr.sin_port);
  }
}

ChatroomServerEpoll::StaticFileResult ChatroomServerEpoll::handleStaticFile(
    const std::string& path) {
  std::string requestedFile = path;
  if (requestedFile == "/" || requestedFile.empty()) {
    requestedFile = "/login.html";
  }
  std::filesystem::path full_path =
      std::filesystem::path(staticDirPath_) / requestedFile.substr(1);
  if (!std::filesystem::exists(full_path)) {
    return {"<html><body><h1>404 Not Found</h1><p>File not found: " +
                requestedFile + "</p></body></html>",
            "text/html"};
  }
  std::ifstream file(full_path, std::ios::binary);
  if (!file) {
    return {
        "<html><body><h1>500 Internal Server Error</h1><p>Failed to read "
        "file</p></body></html>",
        "text/html"};
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  LOG(INFO) << "Serving static file: " << requestedFile;
  return {ss.str(), getContentType(requestedFile)};
}

void ChatroomServerEpoll::handleClientEvent(int clientFd) {
  char buf[8192];
  std::string request;
  while (true) {
    ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
    if (n > 0) {
      request.append(buf, n);
      if (n < (ssize_t)sizeof(buf)) break;  // 简单处理：假设一次收完
    } else if (n == 0) {
      eventLoop_->removeChannel(clientFd);
      pendingDeleteFds_.push_back(clientFd);
      return;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      eventLoop_->removeChannel(clientFd);
      pendingDeleteFds_.push_back(clientFd);
      return;
    }
  }

  // 解析HTTP请求
  std::string method, path, body;
  std::unordered_map<std::string, std::string> headers;
  if (!parseHttpRequest(request, method, path, headers, body)) {
    std::string resp = makeHttpResponse("{\"error\":\"Bad Request\"}",
                                        "application/json", 400);
    send(clientFd, resp.data(), resp.size(), 0);
    eventLoop_->removeChannel(clientFd);
    pendingDeleteFds_.push_back(clientFd);
    return;
  }
  LOG(INFO) << "Received request: " << method << " " << path;

  // 路由分发
  std::string respBody;
  std::string contentType = "application/json";
  bool isStatic = false;

  auto mit = handlers_.find(method);
  if (mit != handlers_.end()) {
    auto pit = mit->second.find(path);
    if (pit != mit->second.end()) {
      respBody = pit->second(body, headers);
    } else if (method == "GET") {
      auto staticResult = handleStaticFile(path);
      respBody = staticResult.content;
      contentType = staticResult.contentType;
      isStatic = true;
    }
  } else if (method == "GET") {
    auto staticResult = handleStaticFile(path);
    respBody = staticResult.content;
    contentType = staticResult.contentType;
    isStatic = true;
  } else {
    respBody = "{\"error\":\"Not found\"}";
  }

  std::string resp = makeHttpResponse(respBody, contentType);
  send(clientFd, resp.data(), resp.size(), 0);
  LOG(INFO) << "Sent response: " << contentType;

  // 短连接，直接关闭
  eventLoop_->removeChannel(clientFd);
  pendingDeleteFds_.push_back(clientFd);
  LOG(INFO) << "Client disconnected: " << clientFd;
}

void ChatroomServerEpoll::setupRoutes() {
  // 静态文件路由已在 handleClientEvent 兜底处理

  // 注册
  registerHandler(
      "POST", "/register", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("username") || !data.contains("password"))
            return std::string("{\"error\":\"Missing username or password\"}");
          std::string username = data["username"];
          std::string password = data["password"];
          if (dbManager_->validateUser(username, password)) {
            LOG(WARN) << "User already exists: " << username;
            return std::string("{\"error\":\"Username already exists\"}");
          }
          if (dbManager_->createUser(username, password)) {
            {
              LOG(INFO) << "User registered: " << username;
              return std::string("{\"status\":\"success\"}");
            }
          }
          LOG(ERROR) << "Failed to create user: " << username;
          return std::string("{\"error\":\"Internal server error\"}");
        } catch (...) {
          LOG(ERROR) << "Failed to parse registration request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 登录
  registerHandler(
      "POST", "/login", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("username") || !data.contains("password"))
            return std::string("{\"error\":\"Missing username or password\"}");
          std::string username = data["username"];
          std::string password = data["password"];
          if (dbManager_->validateUser(username, password)) {
            dbManager_->setUserOnlineStatus(username, true);
            dbManager_->setUserLastActiveTime(username);

            // 添加Kafka事件
            nlohmann::json kafka_event = {
                {"username", username},
                {"action", "login"},
                {"timestamp",
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()},
                {"type", "user_event"}};
            if (kafkaProducer_->send(kafka_event.dump())) {
              LOG(INFO) << "Kafka send success: " << kafka_event.dump();
            } else {
              LOG(ERROR) << "Kafka send failed: " << kafka_event.dump();
            }

            nlohmann::json resp = {{"status", "success"},
                                   {"username", username}};
            LOG(INFO) << "User logged in: " << username;
            return resp.dump();
          } else {
            LOG(WARN) << "Invalid login attempt for user: " << username;
            return std::string("{\"error\":\"Invalid username or password\"}");
          }
        } catch (...) {
          LOG(ERROR) << "Failed to parse login request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 创建房间
  registerHandler(
      "POST", "/create_room", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("name") || !data.contains("creator"))
            return std::string("{\"error\":\"Missing room name or creator\"}");
          std::string room_name = data["name"];
          std::string creator = data["creator"];
          if (dbManager_->createRoom(room_name, creator)) {
            if (dbManager_->addUserToRoom(room_name, creator)) {
              LOG(INFO) << "Room created: " << room_name
                        << " by user: " << creator;
              nlohmann::json kafka_event = {
                  {"room", room_name},
                  {"creator", creator},
                  {"action", "create_room"},
                  {"timestamp",
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()},
                  {"type", "room_event"}};
              if (kafkaProducer_->send(kafka_event.dump())) {
                LOG(INFO) << "Kafka send success: " << kafka_event.dump();
              } else {
                LOG(ERROR) << "Kafka send failed: " << kafka_event.dump();
              }
              return std::string("{\"status\":\"success\"}");
            }
          }
          LOG(ERROR) << "Failed to create room: " << room_name;
          return std::string("{\"error\":\"Failed to create room\"}");
        } catch (...) {
          LOG(ERROR) << "Failed to parse create room request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 加入房间
  registerHandler(
      "POST", "/join_room", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("room") || !data.contains("username"))
            return std::string("{\"error\":\"Missing room or username\"}");
          std::string room_name = data["room"];
          std::string username = data["username"];
          if (dbManager_->addUserToRoom(room_name, username)) {
            LOG(INFO) << "User joined room: " << username << " -> "
                      << room_name;
            return std::string("{\"status\":\"success\"}");
          } else {
            LOG(WARN) << "Room not found: " << room_name;
            return std::string("{\"error\":\"Room not found\"}");
          }
        } catch (...) {
          LOG(ERROR) << "Failed to parse join room request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 获取房间列表
  registerHandler("GET", "/rooms", [this](const std::string&, const auto&) {
    auto rooms = dbManager_->getRooms();
    nlohmann::json response = nlohmann::json::array();
    for (const auto& room : rooms) {
      nlohmann::json room_info = {{"name", room},
                                  {"members", dbManager_->getRoomUsers(room)}};
      response.push_back(room_info);
    }
    LOG(INFO) << "Room list retrieved";
    return response.dump();
  });

  // 发送消息
  registerHandler(
      "POST", "/send_message", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("room") || !data.contains("username") ||
              !data.contains("content"))
            return std::string("{\"error\":\"Missing required fields\"}");
          std::string room_name = data["room"];
          std::string username = data["username"];
          std::string content = data["content"];
          int64_t timestamp =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
          dbManager_->checkAndUpdateInactiveUsers(username);
          if (dbManager_->saveMessage(room_name, username, content,
                                      timestamp)) {
            // Kafka 消息发送
            nlohmann::json kafka_message = {{"room", room_name},
                                            {"username", username},
                                            {"content", content},
                                            {"timestamp", timestamp},
                                            {"type", "chat_message"}};
            if (kafkaProducer_->send(kafka_message.dump())) {
              LOG(INFO) << "Kafka send success: " << kafka_message.dump();
            } else {
              LOG(ERROR) << "Kafka send failed: " << kafka_message.dump();
            }

            LOG(INFO) << "Message sent in room: " << room_name
                      << " from user: " << username;
            return std::string("{\"status\":\"success\"}");
          } else {
            LOG(ERROR) << "Failed to save message in room: " << room_name
                       << " from user: " << username;
            return std::string("{\"error\":\"Failed to save message\"}");
          }
        } catch (...) {
          LOG(ERROR) << "Failed to parse send message request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 获取消息
  registerHandler(
      "POST", "/messages", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (data.contains("username")) {
            dbManager_->checkAndUpdateInactiveUsers(data["username"]);
          }
          if (!data.contains("room") || !data.contains("since"))
            return std::string("{\"error\":\"Missing required fields\"}");
          std::string room_name = data["room"];
          int64_t since =
              data["since"].is_null() ? 0 : data["since"].get<int64_t>();
          auto messages = dbManager_->getRoomMessages(room_name, since);
          if (data.contains("username")) {
            dbManager_->setUserLastActiveTime(data["username"]);
          }
          nlohmann::json resp_json = messages;
          LOG(INFO) << "Retrieved messages for room: " << room_name;
          return resp_json.dump();
        } catch (...) {
          LOG(ERROR) << "Failed to parse get messages request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });

  // 获取用户列表
  registerHandler("GET", "/users", [this](const std::string&, const auto&) {
    auto users = dbManager_->getAllUsers();
    nlohmann::json response = nlohmann::json::array();
    for (const auto& user : users) {
      nlohmann::json user_info = {{"username", user.userName},
                                  {"is_online", user.isOnline}};
      response.push_back(user_info);
    }
    LOG(INFO) << "User list retrieved";
    return response.dump();
  });

  // 登出
  registerHandler(
      "POST", "/logout", [this](const std::string& body, const auto&) {
        try {
          auto data = nlohmann::json::parse(body);
          if (!data.contains("username"))
            return std::string("{\"error\":\"Missing username\"}");
          std::string username = data["username"];
          if (dbManager_->setUserOnlineStatus(username, false)) {
            LOG(INFO) << "User logged out: " << username;
            return std::string("{\"status\":\"success\"}");
          } else {
            LOG(ERROR) << "Failed to log out user: " << username;
            return std::string("{\"error\":\"Internal server error\"}");
          }
        } catch (...) {
          LOG(ERROR) << "Failed to parse logout request";
          return std::string("{\"error\":\"Invalid JSON\"}");
        }
      });
}

void ChatroomServerEpoll::cleanupPendingChannels() {
  for (int fd : pendingDeleteFds_) {
    clientChannels_.erase(fd);
    close(fd);
  }
  pendingDeleteFds_.clear();
}

void ChatroomServerEpoll::registerHandler(const std::string& method,
                                          const std::string& path,
                                          Handler handler) {
  handlers_[method][path] = handler;
}

std::string ChatroomServerEpoll::dispatch(
    const std::string& method, const std::string& path, const std::string& body,
    const std::unordered_map<std::string, std::string>& headers) {
  auto mit = handlers_.find(method);
  if (mit != handlers_.end()) {
    auto pit = mit->second.find(path);
    if (pit != mit->second.end()) {
      LOG(INFO) << "Dispatching request: " << method << " " << path;
      return pit->second(body, headers);
    }
  }
  return "";
}