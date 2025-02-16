#include "chat_application.hpp"
#include "utils/logger.hpp"
#include <filesystem>
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>

using namespace std;
using json = nlohmann::json;

namespace {
// 解析查询参数
std::unordered_map<std::string, std::string> parseQueryParams(const std::string& path) {
    std::unordered_map<std::string, std::string> params;
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
        std::string query = path.substr(pos + 1);
        std::stringstream ss(query);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                params[pair.substr(0, eq_pos)] = pair.substr(eq_pos + 1);
            }
        }
    }
    return params;
}
} // namespace

ChatApplication::ChatApplication(const std::string& static_dir)
    : static_dir_(static_dir)
    , http_server_(nullptr)
    , user_manager_(std::make_shared<UserManager>()) 
    , chat_manager_(std::make_shared<chat::ChatManager>(user_manager_)) {
}

void ChatApplication::start(int port) {
    LOG_INFO << "Starting chat application on port " << port;
    http_server_ = std::make_shared<http::HttpServer>(port);
    setupRoutes();
    http_server_->run();
}

void ChatApplication::stop() {
    LOG_INFO << "Stopping chat application";
    if (http_server_) {
        http_server_->stop();
    }
}

void ChatApplication::setupRoutes() {
    // 主页重定向到登录页
    http_server_->addHandler("/", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        http::HttpResponse response(302);
        response.headers["Location"] = "/login.html";
        return response;
    });
    
    // 用户注册
    http_server_->addHandler("/register", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        return handleRegister(request);
    });
    
    // 用户登录
    http_server_->addHandler("/login", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        auto data = json::parse(request.body);
        std::string username = data["username"];
        std::string password = data["password"];
        
        if (user_manager_->loginUser(username, password)) {
            LOG_INFO << "User logged in: " << username;
            return http::HttpResponse(200, "{\"message\":\"Login successful\"}");
        }
        
        LOG_WARN << "Login failed for user: " << username;
        return http::HttpResponse(401, "{\"error\":\"Invalid credentials\"}");
    });
    
    // 自动登录
    http_server_->addHandler("/auto_login", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /auto_login request";
        try {
            auto data = json::parse(request.body);
            
            if (!data.contains("username") || !data.contains("password")) {
                LOG_ERROR << "Missing required fields in auto login request";
                return http::HttpResponse(400, "{\"error\":\"Missing username or password\"}");
            }
            
            std::string username = data["username"];
            std::string password = data["password"];
            
            if (user_manager_->verifyCredentials(username, password)) {
                LOG_INFO << "Auto login successful for user: " << username;
                return http::HttpResponse(200, "{\"message\":\"Auto login successful\"}");
            } else {
                LOG_WARN << "Auto login failed for user: " << username;
                return http::HttpResponse(401, "{\"error\":\"Invalid credentials\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "Invalid JSON format in auto login request: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON format\"}");
        } catch (const std::exception& e) {
            LOG_ERROR << "Error in auto login request: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // 创建聊天室
    http_server_->addHandler("/create_room", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /create_room request";
        try {
            auto data = json::parse(request.body);
            LOG_DEBUG << "Parsed request body: " << data.dump();
            
            if (!data.contains("name") || !data.contains("creator")) {
                LOG_ERROR << "Missing required fields in create room request";
                return http::HttpResponse(400, "{\"error\":\"Missing room name or creator\"}");
            }
            
            std::string room_name = data["name"];
            std::string creator = data["creator"];
            
            if (chat_manager_->createRoom(room_name, creator)) {
                LOG_INFO << "Successfully created room '" << room_name << "'";
                
                // 返回完整的房间信息，包括消息历史
                json response = {
                    {"message", "Room created successfully"},
                    {"room", room_name},
                    {"creator", creator},
                    {"messages", chat_manager_->getRoomMessages(room_name)}
                };
                return http::HttpResponse(200, response.dump());
            } else {
                LOG_WARN << "Failed to create room '" << room_name << "' (already exists)";
                return http::HttpResponse(409, "{\"error\":\"Room already exists\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "Invalid JSON format: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON format\"}");
        } catch (const std::exception& e) {
            LOG_ERROR << "Error creating room: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // 加入聊天室
    http_server_->addHandler("/join_room", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /join_room request";
        try {
            auto data = json::parse(request.body);
            LOG_DEBUG << "Parsed request body: " << data.dump();
            
            if (!data.contains("room") || !data.contains("username")) {
                LOG_ERROR << "Missing required fields in join room request";
                return http::HttpResponse(400, "{\"error\":\"Missing room name or username\"}");
            }
            
            std::string room_name = data["room"];
            std::string username = data["username"];
            
            auto room = chat_manager_->findRoom(room_name);
            if (!room) {
                LOG_WARN << "Room not found: " << room_name;
                return http::HttpResponse(404, "{\"error\":\"Room not found\"}");
            }
            
            if (chat_manager_->joinRoom(room_name, username)) {
                LOG_INFO << "User '" << username << "' successfully joined room '" << room_name << "'";
                json response = {
                    {"message", "Successfully joined room"},
                    {"room", room_name},
                    {"messages", chat_manager_->getRoomMessages(room_name)}
                };
                return http::HttpResponse(200, response.dump());
            } else {
                LOG_INFO << "User '" << username << "' is already in room '" << room_name << "'";
                json response = {
                    {"message", "Already in room"},
                    {"room", room_name},
                    {"messages", chat_manager_->getRoomMessages(room_name)}
                };
                return http::HttpResponse(200, response.dump());
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "Invalid JSON format: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON format\"}");
        } catch (const std::exception& e) {
            LOG_ERROR << "Error joining room: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // 获取聊天室列表
    http_server_->addHandler("/rooms", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /rooms request";
        try {
            auto rooms = chat_manager_->getRoomList();
            json response = json::array();
            for (const auto& room_name : rooms) {
                auto room = chat_manager_->findRoom(room_name);
                if (room) {
                    json room_info = {
                        {"name", room_name},
                        {"creator", room->getCreator()},
                        {"members", room->getMembers()},
                        {"message_count", room->getMessages().size()}
                    };
                    response.push_back(room_info);
                }
            }
            return http::HttpResponse(200, response.dump());
        } catch (const std::exception& e) {
            LOG_ERROR << "Error getting room list: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // 发送消息
    http_server_->addHandler("/send_message", "POST", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /send_message request";
        try {
            auto data = json::parse(request.body);
            LOG_DEBUG << "Parsed request body: " << data.dump();
            
            if (!data.contains("room") || !data.contains("sender") || !data.contains("content")) {
                LOG_ERROR << "Missing required fields in send message request";
                return http::HttpResponse(400, "{\"error\":\"Missing room, sender or content\"}");
            }
            
            std::string room_name = data["room"];
            std::string sender = data["sender"];
            std::string content = data["content"];
            
            if (chat_manager_->sendMessage(room_name, sender, content)) {
                LOG_INFO << "Message sent to room '" << room_name << "' by '" << sender << "'";
                json response = {
                    {"message", "Message sent successfully"},
                    {"room", room_name},
                    {"messages", chat_manager_->getRoomMessages(room_name)}
                };
                return http::HttpResponse(200, response.dump());
            } else {
                LOG_WARN << "Failed to send message to room '" << room_name << "'";
                return http::HttpResponse(404, "{\"error\":\"Not in room\"}");
            }
        } catch (const json::exception& e) {
            LOG_ERROR << "Invalid JSON format: " << e.what();
            return http::HttpResponse(400, "{\"error\":\"Invalid JSON format\"}");
        } catch (const std::exception& e) {
            LOG_ERROR << "Error sending message: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // 获取消息
    http_server_->addHandler("/messages", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        LOG_INFO << "Handling /messages request";
        try {
            auto params = parseQueryParams(request.path);
            auto it = params.find("room");
            if (it == params.end()) {
                LOG_ERROR << "Missing room parameter in get messages request";
                return http::HttpResponse(400, "{\"error\":\"Missing room parameter\"}");
            }
            
            std::string room_name = it->second;
            auto messages = chat_manager_->getRoomMessages(room_name);
            
            json response = json::array();
            for (const auto& msg : messages) {
                response.push_back(msg);
            }
            
            return http::HttpResponse(200, response.dump());
        } catch (const std::exception& e) {
            LOG_ERROR << "Error getting messages: " << e.what();
            return http::HttpResponse(500, "{\"error\":\"Internal server error\"}");
        }
    });
    
    // index.html 处理
    http_server_->addHandler("/index.html", "GET", [this](const http::HttpRequest& request) -> http::HttpResponse {
        std::string file_path = static_dir_ + "/index.html";
        if (std::filesystem::exists(file_path)) {
            std::ifstream file(file_path);
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            return http::HttpResponse(200, buffer.str());
        } else {
            return http::HttpResponse(404, "{\"error\":\"File not found\"}");
        }
    });
}

http::HttpResponse ChatApplication::handleRegister(const http::HttpRequest& request) {
    LOG_DEBUG << "Handling register request with body: " << request.body;
    
    try {
        auto json = nlohmann::json::parse(request.body);
        std::string username = json["username"];
        std::string password = json["password"];
        
        LOG_INFO << "Registering user: " << username;
        
        if (user_manager_->registerUser(username, password)) {
            LOG_INFO << "User registered successfully: " << username;
            auto response = http::HttpResponse(200, "{\"status\":\"success\",\"message\":\"User registered successfully\"}");
            LOG_DEBUG << "Sending response: " << response.toString();
            return response;
        } else {
            LOG_WARN << "Failed to register user: " << username;
            auto response = http::HttpResponse(400, "{\"status\":\"error\",\"message\":\"Username already exists\"}");
            LOG_DEBUG << "Sending response: " << response.toString();
            return response;
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to parse register request: " << e.what();
        auto response = http::HttpResponse(400, "{\"status\":\"error\",\"message\":\"Invalid request format\"}");
        LOG_DEBUG << "Sending response: " << response.toString();
        return response;
    }
}
