#include "chat_application.hpp"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

#include "utils/logger.hpp"

using json = nlohmann::json;

ChatApplication::ChatApplication(const std::string& static_dir)
    : static_dir_(static_dir),
      http_server_(nullptr)
      // , user_manager_(std::make_shared<UserManager>())
      // , chat_manager_(std::make_shared<chat::ChatManager>())
      ,
      db_manager_(std::make_shared<DatabaseManager>("chat.db")) {}

void ChatApplication::start(int port) {
  LOG_INFO << "Creating HTTP server on port " << port;
  http_server_ = std::make_unique<http::ReactorServer>(port);

  LOG_INFO << "Setting up routes";
  setupRoutes();

  LOG_INFO << "Starting HTTP server";
  http_server_->run();
}

void ChatApplication::stop() {
  if (http_server_) {
    http_server_->stop();
  }
}

/**
 * @brief 设置HTTP路由规则
 *
 * 定义各API端点及其处理函数
 */
void ChatApplication::setupRoutes() {
  // 根路径处理 - 返回登录页面
  http_server_->addHandler(
      "/", "GET",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        return serveStaticFile("/login.html");
      });

  /**
   * @brief 处理用户注册请求
   *
   * 接收JSON格式的用户名和密码，创建新用户
   * POST /register
   * Body: {"username": "用户名", "password": "密码"}
   */
  http_server_->addHandler(
      "/register", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("username") || !data.contains("password")) {
            LOG_ERROR << "Missing username or password in register request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing username or password\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (db_manager_->userExists(username)) {
            LOG_WARN << "Username already exists: " << username;
            return http::HttpResponse(
                400, "{\"error\":\"Username already exists\"}");
          }

          if (db_manager_->createUser(username, password)) {
            LOG_INFO << "User registered: " << username;
            return http::HttpResponse(200, "{\"status\":\"success\"}");
          } else {
            LOG_ERROR << "Failed to create user in database: " << username;
            return http::HttpResponse(500,
                                      "{\"error\":\"Internal server error\"}");
          }
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 处理用户登录请求
   *
   * 验证用户名和密码，更新用户在线状态
   * POST /login
   * Body: {"username": "用户名", "password": "密码"}
   */
  http_server_->addHandler(
      "/login", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("username") || !data.contains("password")) {
            LOG_ERROR << "Missing username or password in login request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing username or password\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (db_manager_->validateUser(username, password)) {
            LOG_INFO << "User logged in: " << username;
            db_manager_->setUserOnlineStatus(username, true);
            db_manager_->updateUserLastActiveTime(username);
            json response = {{"status", "success"}, {"username", username}};
            return http::HttpResponse(200, response.dump());
          } else {
            LOG_WARN << "Invalid login attempt for user: " << username;
            return http::HttpResponse(
                401, "{\"error\":\"Invalid username or password\"}");
          }
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 创建聊天室
   *
   * 创建新聊天室并添加创建者为成员
   * POST /create_room
   * Body: {"name": "房间名", "creator": "创建者用户名"}
   */
  http_server_->addHandler(
      "/create_room", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("name") || !data.contains("creator")) {
            LOG_ERROR << "Missing room name or creator in create room request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing room name or creator\"}");
          }

          std::string room_name = data["name"];
          std::string creator = data["creator"];

          if (db_manager_->createRoom(room_name, creator)) {
            if (db_manager_->addRoomMember(room_name, creator)) {
              LOG_INFO << "Created room and added creator: " << room_name
                       << ", " << creator;
              return http::HttpResponse(200, "{\"status\":\"success\"}");
            }
          }
          LOG_ERROR << "Failed to create room: " << room_name;
          return http::HttpResponse(500,
                                    "{\"error\":\"Failed to create room\"}");
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 加入聊天室
   *
   * 将用户添加到指定聊天室
   * POST /join_room
   * Body: {"room": "房间名", "username": "用户名"}
   */
  http_server_->addHandler(
      "/join_room", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("room") || !data.contains("username")) {
            LOG_ERROR << "Missing room or username in join room request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing room or username\"}");
          }

          std::string room_name = data["room"];
          std::string username = data["username"];

          if (db_manager_->addRoomMember(room_name, username)) {
            LOG_INFO << "User " << username << " joined room: " << room_name;
            return http::HttpResponse(200, "{\"status\":\"success\"}");
          } else {
            LOG_WARN << "Failed to join room: " << room_name;
            return http::HttpResponse(404, "{\"error\":\"Room not found\"}");
          }
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 获取聊天室列表
   *
   * 返回所有聊天室及其成员
   * GET /rooms
   */
  http_server_->addHandler(
      "/rooms", "GET",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        auto rooms = db_manager_->getRooms();
        json response = json::array();

        for (const auto& room : rooms) {
          json room_info = {{"name", room},
                            {"members", db_manager_->getRoomMembers(room)}};
          response.push_back(room_info);
        }

        return http::HttpResponse(200, response.dump());
      });

  /**
   * @brief 发送消息
   *
   * 在指定聊天室发送新消息
   * POST /send_message
   * Body: {"room": "房间名", "username": "用户名", "content": "消息内容"}
   */
  http_server_->addHandler(
      "/send_message", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("room") || !data.contains("username") ||
              !data.contains("content")) {
            LOG_ERROR << "Missing required fields in send message request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing required fields\"}");
          }

          std::string room_name = data["room"];
          std::string username = data["username"];
          std::string content = data["content"];
          int64_t timestamp =
              std::chrono::system_clock::now().time_since_epoch().count();

          if (db_manager_->saveMessage(room_name, username, content,
                                       timestamp)) {
            LOG_INFO << "Message saved from " << username << " in room "
                     << room_name;
            return http::HttpResponse(200, "{\"status\":\"success\"}");
          } else {
            LOG_ERROR << "Failed to save message";
            return http::HttpResponse(500,
                                      "{\"error\":\"Failed to save message\"}");
          }
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 获取新消息
   *
   * 获取指定聊天室在给定时间戳之后的所有消息
   * POST /messages
   * Body: {"room": "房间名", "since": 时间戳, "username": "用户名"}
   */
  http_server_->addHandler(
      "/messages", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("room") || !data.contains("since")) {
            LOG_ERROR
                << "Missing room or since timestamp in get messages request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing required fields\"}");
          }

          std::string room_name = data["room"];
          int64_t since =
              data["since"].is_null() ? 0 : data["since"].get<int64_t>();

          auto messages = db_manager_->getMessages(room_name, since);
          // 更新用户最后活动时间
          if (data.contains("username")) {
            db_manager_->updateUserLastActiveTime(data["username"]);
          }
          return http::HttpResponse(200, json(messages).dump());
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  /**
   * @brief 获取用户列表
   *
   * 返回所有用户及其在线状态
   * GET /users
   */
  http_server_->addHandler(
      "/users", "GET",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /users request";
        try {
          auto users = db_manager_->getAllUsers();
          json response = json::array();

          LOG_INFO << "Found " << users.size() << " users";

          for (const auto& user : users) {
            json user_info = {{"username", user.username},
                              {"is_online", user.is_online}};
            response.push_back(user_info);
          }

          std::string response_str = response.dump();
          LOG_INFO << "Response: " << response_str;

          return http::HttpResponse(200, response_str);
        } catch (const std::exception& e) {
          LOG_ERROR << "Error getting user list: " << e.what();
          return http::HttpResponse(500,
                                    "{\"error\":\"Internal server error\"}");
        }
      });

  /**
   * @brief 处理用户登出请求
   *
   * 更新用户在线状态为离线
   * POST /logout
   * Body: {"username": "用户名"}
   */
  http_server_->addHandler(
      "/logout", "POST",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          json data = json::parse(request.body);

          if (!data.contains("username")) {
            LOG_ERROR << "Missing username in logout request";
            return http::HttpResponse(400, "{\"error\":\"Missing username\"}");
          }

          std::string username = data["username"];

          if (db_manager_->setUserOnlineStatus(username, false)) {
            LOG_INFO << "User logged out: " << username;
            return http::HttpResponse(200, "{\"status\":\"success\"}");
          } else {
            LOG_ERROR << "Failed to logout user: " << username;
            return http::HttpResponse(500,
                                      "{\"error\":\"Internal server error\"}");
          }
        } catch (const json::exception& e) {
          LOG_ERROR << "JSON parse error in logout: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });
}

/**
 * @brief 提供静态文件服务
 *
 * @param path 请求的文件路径
 * @return http::HttpResponse 包含文件内容的HTTP响应
 */
http::HttpResponse ChatApplication::serveStaticFile(const std::string& path) {
  std::string full_path = static_dir_ + path;

  struct stat st;
  if (stat(full_path.c_str(), &st) == -1) {
    if (errno == ENOENT) {
      LOG_WARN << "File not found: " << full_path;
      return http::HttpResponse(404, "File not found");
    }
    LOG_ERROR << "Failed to check file: " << full_path
              << ", error: " << strerror(errno);
    return http::HttpResponse(500, "Internal server error");
  }

  std::ifstream file(full_path, std::ios::binary);
  if (!file) {
    LOG_ERROR << "Failed to open file: " << full_path;
    return http::HttpResponse(500, "Failed to open file");
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

  // 根据文件扩展名设置 Content-Type
  std::string content_type = "text/plain";
  size_t dot_pos = path.find_last_of('.');
  if (dot_pos != std::string::npos) {
    std::string ext = path.substr(dot_pos);
    if (ext == ".html") {
      content_type = "text/html";
    } else if (ext == ".css") {
      content_type = "text/css";
    } else if (ext == ".js") {
      content_type = "application/javascript";
    } else if (ext == ".json") {
      content_type = "application/json";
    } else if (ext == ".png") {
      content_type = "image/png";
    } else if (ext == ".jpg" || ext == ".jpeg") {
      content_type = "image/jpeg";
    }
  }

  http::HttpResponse response(200, content);
  response.headers["Content-Type"] = content_type;
  return response;
}
