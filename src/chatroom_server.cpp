#include "chatroom_server.hpp"

#include <filesystem>  // 添加：用于文件系统操作
#include <fstream>
#include <iostream>
#include <sstream>

#include "utils/logger.hpp"

ChatroomServer::ChatroomServer(const std::string& static_dir_path,
                               const std::string& db_file_path)
    : staticDirPath_(static_dir_path),
      dbManager_(std::make_shared<DatabaseManager>(db_file_path)) {
  LOG(INFO) << "Static directory: " << staticDirPath_;
}

void ChatroomServer::startServer(int port) {
  httpServer_ = std::make_unique<http::HttpServer>(port);
  setupRoutes();
  LOG(INFO) << "ChatroomServer started on port " << port;  // 修正拼写错误
  httpServer_->run();
}

void ChatroomServer::stopServer() {
  if (httpServer_) {
    httpServer_->stop();
    LOG(INFO) << "ChatroomServer stopped";
  }
}

void ChatroomServer::setupRoutes() {
  httpServer_->addHandler("GET", "/", [this](const http::HttpRequest& request) {
    return handleStaticFileRequest(request.path());
  });
  httpServer_->addHandler("GET", "/*",
                          [this](const http::HttpRequest& request) {
                            return handleStaticFileRequest(request.path());
                          });

  httpServer_->addHandler(
      "POST", "/register",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());

          if (!data.contains("username") || !data.contains("password")) {
            LOG(ERROR) << "Missing username or password in register request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing username or password\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (dbManager_->validateUser(username, password)) {
            LOG(WARN) << "Username already exists: " << username;
            return http::HttpResponse(
                400, "{\"error\":\"Username already exists\"}");
          }

          if (dbManager_->createUser(username, password)) {
            LOG(INFO) << "User registered: " << username;
            http::HttpResponse resp(200, "{\"status\":\"success\"}");
            resp.setHeader("Content-Type", "application/json");
            return resp;
          } else {
            LOG(ERROR) << "Failed to create user in database: " << username;
            return http::HttpResponse(500,
                                      "{\"error\":\"Internal server error\"}");
          }
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "POST", "/login",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());

          if (!data.contains("username") || !data.contains("password")) {
            LOG(ERROR) << "Missing username or password in login request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing username or password\"}");
          }

          std::string username = data["username"];
          std::string password = data["password"];

          if (dbManager_->validateUser(username, password)) {
            LOG(INFO) << "User logged in: " << username;
            dbManager_->setUserOnlineStatus(username, true);
            dbManager_->setUserLastActiveTime(username);
            nlohmann::json response = {{"status", "success"},
                                       {"username", username}};
            http::HttpResponse resp(200, response.dump());
            resp.setHeader("Content-Type", "application/json");
            return resp;
          } else {
            LOG(WARN) << "Invalid login attempt for user: " << username;
            return http::HttpResponse(
                401, "{\"error\":\"Invalid username or password\"}");
          }
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "POST", "/create_room",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());

          if (!data.contains("name") || !data.contains("creator")) {
            LOG(ERROR) << "Missing room name or creator in create room request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing room name or creator\"}");
          }

          std::string room_name = data["name"];
          std::string creator = data["creator"];

          if (dbManager_->createRoom(room_name, creator)) {
            if (dbManager_->addUserToRoom(room_name, creator)) {
              LOG(INFO) << "Created room and added creator: " << room_name
                        << ", " << creator;
              http::HttpResponse resp(200, "{\"status\":\"success\"}");
              resp.setHeader("Content-Type", "application/json");
              return resp;
            }
          }
          LOG(ERROR) << "Failed to create room: " << room_name;
          return http::HttpResponse(500,
                                    "{\"error\":\"Failed to create room\"}");
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "POST", "/join_room",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());

          if (!data.contains("room") || !data.contains("username")) {
            LOG(ERROR) << "Missing room or username in join room request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing room or username\"}");
          }

          std::string room_name = data["room"];
          std::string username = data["username"];

          if (dbManager_->addUserToRoom(room_name, username)) {
            LOG(INFO) << "User " << username << " joined room: " << room_name;
            http::HttpResponse resp(200, "{\"status\":\"success\"}");
            resp.setHeader("Content-Type", "application/json");
            return resp;
          } else {
            LOG(WARN) << "Failed to join room: " << room_name;
            return http::HttpResponse(404, "{\"error\":\"Room not found\"}");
          }
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "GET", "/rooms",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        auto rooms = dbManager_->getRooms();
        nlohmann::json response = nlohmann::json::array();

        for (const auto& room : rooms) {
          nlohmann::json room_info = {
              {"name", room}, {"members", dbManager_->getRoomUsers(room)}};
          response.push_back(room_info);
        }

        http::HttpResponse resp(200, response.dump());
        resp.setHeader("Content-Type", "application/json");
        return resp;
      });

  httpServer_->addHandler(
      "POST", "/send_message",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());
          if (!data.contains("room") || !data.contains("username") ||
              !data.contains("content")) {
            LOG(ERROR) << "Missing required fields in send message request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing required fields\"}");
          }

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
            LOG(INFO) << "Message saved from " << username << " in room "
                      << room_name;
            http::HttpResponse resp(200, "{\"status\":\"success\"}");
            resp.setHeader("Content-Type", "application/json");
            return resp;
          } else {
            LOG(ERROR) << "Failed to save message";
            return http::HttpResponse(500,
                                      "{\"error\":\"Failed to save message\"}");
          }
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "POST", "/messages",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());
          if (data.contains("username")) {
            dbManager_->checkAndUpdateInactiveUsers(data["username"]);
          }
          if (!data.contains("room") || !data.contains("since")) {
            LOG(ERROR)
                << "Missing room or since timestamp in get messages request";
            return http::HttpResponse(
                400, "{\"error\":\"Missing required fields\"}");
          }

          std::string room_name = data["room"];
          int64_t since =
              data["since"].is_null() ? 0 : data["since"].get<int64_t>();

          auto messages = dbManager_->getRoomMessages(room_name, since);
          // 更新用户最后活动时间
          if (data.contains("username")) {
            dbManager_->setUserLastActiveTime(data["username"]);
          }

          nlohmann::json resp_json = messages;
          http::HttpResponse resp(200, resp_json.dump());
          resp.setHeader("Content-Type", "application/json");
          return resp;
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });

  httpServer_->addHandler(
      "GET", "/users",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG(INFO) << "Handling /users request";
        try {
          auto users = dbManager_->getAllUsers();
          nlohmann::json response = nlohmann::json::array();

          LOG(INFO) << "Found " << users.size() << " users";

          for (const auto& user : users) {
            nlohmann::json user_info = {{"username", user.userName},
                                        {"is_online", user.isOnline}};
            response.push_back(user_info);
          }

          std::string response_str = response.dump();
          LOG(INFO) << "Response: " << response_str;

          http::HttpResponse resp(200, response_str);
          resp.setHeader("Content-Type", "application/json");
          return resp;
        } catch (const std::exception& e) {
          LOG(ERROR) << "Error getting user list: " << e.what();
          return http::HttpResponse(500,
                                    "{\"error\":\"Internal server error\"}");
        }
      });

  httpServer_->addHandler(
      "POST", "/logout",
      [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
          nlohmann::json data = nlohmann::json::parse(request.body());

          if (!data.contains("username")) {
            LOG(ERROR) << "Missing username in logout request";
            return http::HttpResponse(400, "{\"error\":\"Missing username\"}");
          }

          std::string username = data["username"];

          if (dbManager_->setUserOnlineStatus(username, false)) {
            LOG(INFO) << "User logged out: " << username;
            http::HttpResponse resp(200, "{\"status\":\"success\"}");
            resp.setHeader("Content-Type", "application/json");
            return resp;
          } else {
            LOG(ERROR) << "Failed to logout user: " << username;
            return http::HttpResponse(500,
                                      "{\"error\":\"Internal server error\"}");
          }
        } catch (const nlohmann::json::exception& e) {
          LOG(ERROR) << "JSON parse error in logout: " << e.what();
          return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
      });
}

http::HttpResponse ChatroomServer::handleStaticFileRequest(
    const std::string& dir_path) {
  std::string requestedFile = dir_path;
  if (requestedFile == "/" || requestedFile.empty()) {
    requestedFile = "/login.html";
  }

  // 构建完整路径
  std::filesystem::path full_path =
      std::filesystem::path(staticDirPath_) / requestedFile.substr(1);

  LOG(DEBUG) << "Requesting file: " << requestedFile;
  LOG(DEBUG) << "Full path: " << full_path.string();

  // 检查文件是否存在
  if (!std::filesystem::exists(full_path)) {
    LOG(WARN) << "File not found: " << full_path;
    http::HttpResponse response;
    response.setStatus(404);
    response.setHeader("Content-Type", "text/html");
    response.setBody("<html><body><h1>404 Not Found</h1><p>File not found: " +
                     requestedFile + "</p></body></html>");
    return response;
  }

  // 打开文件
  std::ifstream file(full_path, std::ios::binary);
  if (!file) {
    LOG(ERROR) << "Failed to open file: " << full_path;
    http::HttpResponse response;
    response.setStatus(500);
    response.setHeader("Content-Type", "text/html");
    response.setBody(
        "<html><body><h1>500 Internal Server Error</h1><p>Failed to read "
        "file</p></body></html>");
    return response;
  }

  // 读取文件内容
  std::ostringstream ss;
  ss << file.rdbuf();
  file.close();

  // 确定Content-Type
  std::string contentType = "text/plain";
  auto ends_with = [](const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  };
  if (ends_with(requestedFile, ".html") || ends_with(requestedFile, ".htm")) {
    contentType = "text/html";
  } else if (ends_with(requestedFile, ".css")) {
    contentType = "text/css";
  } else if (ends_with(requestedFile, ".js")) {
    contentType = "application/javascript";
  } else if (ends_with(requestedFile, ".json")) {
    contentType = "application/json";
  } else if (ends_with(requestedFile, ".png")) {
    contentType = "image/png";
  } else if (ends_with(requestedFile, ".jpg") ||
             ends_with(requestedFile, ".jpeg")) {
    contentType = "image/jpeg";
  }

  LOG(INFO) << "Serving file: " << full_path << " (" << ss.str().length()
            << " bytes, " << contentType << ")";

  // 创建响应
  http::HttpResponse response;
  response.setStatus(200);
  response.setHeader("Content-Type", contentType);
  response.setBody(ss.str());

  return response;
}