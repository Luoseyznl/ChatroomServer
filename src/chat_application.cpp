#include "chat_application.hpp"
#include "utils/logger.hpp"
#include <fstream>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChatApplication::ChatApplication(const std::string& static_dir)
    : static_dir_(static_dir)
    , http_server_(nullptr)
    , user_manager_(std::make_shared<UserManager>())
    , chat_manager_(std::make_shared<chat::ChatManager>())
{
    // 移除这里的 setupRoutes 调用，因为 http_server_ 还是 nullptr
}

void ChatApplication::start(int port) {
    LOG_INFO << "Creating HTTP server on port " << port;
    http_server_ = std::make_unique<http::HttpServer>(port);
    
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

void ChatApplication::setupRoutes() {
    // 静态文件处理
    http_server_->addHandler("/", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        return serveStaticFile("/index.html");
    });
    
    // 处理注册请求
    http_server_->addHandler("/register", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
            json data = json::parse(request.body);
            
            if (!data.contains("username") || !data.contains("password")) {
                LOG_ERROR << "Missing username or password in register request";
                return http::HttpResponse(400, "{\"error\":\"Missing username or password\"}");
            }
            
            std::string username = data["username"];
            std::string password = data["password"];
            
            if (user_manager_->registerUser(username, password)) {
                LOG_INFO << "User registered: " << username;
                return http::HttpResponse(200, "{\"status\":\"success\"}");
            } else {
                LOG_WARN << "Failed to register user: " << username;
                return http::HttpResponse(400, "{\"error\":\"Username already exists\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 处理登录请求
    http_server_->addHandler("/login", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
         try {
                json data = json::parse(request.body);
                
                if (!data.contains("username") || !data.contains("password")) {
                    LOG_ERROR << "Missing username or password in login request";
                    return http::HttpResponse(400, "{\"error\":\"Missing username or password\"}");
                }
                
                std::string username = data["username"];
                std::string password = data["password"];
                
                if (user_manager_->loginUser(username, password)) {
                    LOG_INFO << "User logged in: " << username;
                    json response = {
                        {"status", "success"},
                        {"username", username}
                    };
                    return http::HttpResponse(200, response.dump());
                } else {
                    LOG_WARN << "Failed to log in user: " << username;
                    return http::HttpResponse(401, "{\"error\":\"Invalid username or password\"}");
                }
            } catch (const json::exception& e) {
                LOG_ERROR << "JSON parse error: " << e.what();
                return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
            }
    });
    
    // 自动登录
    http_server_->addHandler("/auto_login", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        try {
            json data = json::parse(request.body);
            LOG_DEBUG << "Auto login request: " << data.dump();
            
            if (!data.contains("username")) {
                LOG_ERROR << "Missing username in auto login request";
                return http::HttpResponse(400, "{\"error\":\"Missing username\"}");
            }
            
            std::string username = data["username"];
            if (user_manager_->userExists(username)) {
                LOG_INFO << "Auto login successful for user: " << username;
                json response = {
                    {"status", "success"},
                    {"username", username}
                };
                return http::HttpResponse(200, response.dump());
            } else {
                LOG_WARN << "Auto login failed: user not found: " << username;
                return http::HttpResponse(401, "{\"error\":\"User not found\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error in auto login: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 创建聊天室
    http_server_->addHandler("/create_room", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /create_room request";
        try {
            json data = json::parse(request.body);
            
            if (!data.contains("name") || !data.contains("creator")) {
                LOG_ERROR << "Missing room name or creator in create room request";
                return http::HttpResponse(400, "{\"error\":\"Missing room name or creator\"}");
            }
            
            std::string room_name = data["name"];
            std::string creator = data["creator"];
            
            if (chat_manager_->createRoom(room_name, creator)) {
                LOG_INFO << "Created room: " << room_name;
                return http::HttpResponse(200, "{\"status\":\"success\"}");
            } else {
                LOG_WARN << "Failed to create room: " << room_name;
                return http::HttpResponse(400, "{\"error\":\"Room already exists\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 加入聊天室
    http_server_->addHandler("/join_room", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /join_room request";
        try {
            json data = json::parse(request.body);
            LOG_DEBUG << "Join room request data: " << data.dump();
            
            if (!data.contains("room") || !data.contains("username")) {
                LOG_ERROR << "Missing room or username in join room request";
                return http::HttpResponse(400, "{\"error\":\"Missing room or username\"}");
            }
            
            std::string room_name = data["room"];
            std::string username = data["username"];
            
            if (chat_manager_->joinRoom(room_name, username)) {
                LOG_INFO << "User " << username << " joined room: " << room_name;
                return http::HttpResponse(200, "{\"status\":\"success\"}");
            } else {
                LOG_WARN << "Failed to join room: " << room_name << " for user: " << username;
                return http::HttpResponse(404, "{\"error\":\"Room not found\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 获取房间列表
    http_server_->addHandler("/rooms", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /rooms request";
        auto rooms = chat_manager_->getRooms();
        json response = json::array();
        for (const auto& room : rooms) {
            json room_info = {
                {"name", room},
                {"members", chat_manager_->getRoomMembers(room)}
            };
            response.push_back(room_info);
        }
        LOG_DEBUG << "Found " << rooms.size() << " rooms: " << response.dump();
        return http::HttpResponse(200, response.dump());
    });
    
    // 处理发送消息请求
    http_server_->addHandler("/send_message", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /send_message request";
        try {
            json data = json::parse(request.body);
            LOG_DEBUG << "Message data: " << data.dump();
            
            if (!data.contains("room") || !data.contains("username") || !data.contains("content")) {
                LOG_ERROR << "Missing required fields in send message request";
                return http::HttpResponse(400, "{\"error\":\"Missing room, username or content\"}");
            }
            
            std::string room_name = data["room"];
            std::string username = data["username"];
            json content = data["content"];
            
            if (!chat_manager_->isUserInRoom(room_name, username)) {
                LOG_WARN << "User " << username << " is not in room " << room_name;
                return http::HttpResponse(403, "{\"error\":\"User is not in the room\"}");
            }
            
            if (chat_manager_->sendMessage(room_name, username, content)) {
                LOG_INFO << "Message sent in room " << room_name << " by " << username;
                return http::HttpResponse(200, "{\"status\":\"success\"}");
            } else {
                LOG_WARN << "Failed to send message in room " << room_name;
                return http::HttpResponse(500, "{\"error\":\"Failed to send message\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 获取新消息
    http_server_->addHandler("/messages", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /messages request";
        try {
            json data = json::parse(request.body);
            LOG_DEBUG << "Get messages request data: " << data.dump();
            
            if (!data.contains("room") || !data.contains("username")) {
                LOG_ERROR << "Missing room or username in get messages request";
                return http::HttpResponse(400, "{\"error\":\"Missing room or username\"}");
            }
            
            std::string room_name = data["room"];
            std::string username = data["username"];
            int64_t since = data.value("since", 0);  // 获取时间戳，默认为0
            
            if (!chat_manager_->isUserInRoom(room_name, username)) {
                LOG_WARN << "User " << username << " is not in room " << room_name;
                return http::HttpResponse(403, "{\"error\":\"User is not in the room\"}");
            }
            
            auto messages = chat_manager_->getNewMessages(room_name, username);
            
            // 过滤消息
            json filtered_messages = json::array();
            for (const auto& msg : messages) {
                if (msg["timestamp"].get<int64_t>() > since) {
                    filtered_messages.push_back(msg);
                }
            }
            
            LOG_DEBUG << "Found " << filtered_messages.size() << " new messages for user " << username 
                     << " in room " << room_name << " since " << since;
            
            return http::HttpResponse(200, filtered_messages.dump());
        } catch (const json::exception& e) {
            LOG_ERROR << "JSON parse error: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON\"}");
        }
    });
    
    // 获取用户列表
    http_server_->addHandler("/users", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /users request";
        try {
            auto users = user_manager_->getAllUsers();
            json response = json::array();
            
            LOG_INFO << "Found " << users.size() << " users";
            
            for (const auto& user : users) {
                json user_info = {
                    {"username", user.username}
                };
                response.push_back(user_info);
            }
            
            std::string response_str = response.dump();
            LOG_INFO << "Response: " << response_str;
            
            return http::HttpResponse(200, response_str);
        } catch (const std::exception& e) {
            LOG_ERROR << "Error getting user list: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
}


http::HttpResponse ChatApplication::serveStaticFile(const std::string& path) {
    std::string full_path = static_dir_ + path;
    
    struct stat st;
    if (stat(full_path.c_str(), &st) == -1) {
        if (errno == ENOENT) {
            LOG_WARN << "File not found: " << full_path;
            return http::HttpResponse(404, "File not found");
        }
        LOG_ERROR << "Failed to check file: " << full_path << ", error: " << strerror(errno);
        return http::HttpResponse(500, "Internal server error");
    }
    
    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        LOG_ERROR << "Failed to open file: " << full_path;
        return http::HttpResponse(500, "Failed to open file");
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
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
