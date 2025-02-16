#pragma once

#include "http/http_server.hpp"
#include "chat/chat_manager.hpp"
#include "user/user_manager.hpp"
#include <memory>
#include <string>

class ChatApplication {
public:
    explicit ChatApplication(const std::string& static_dir);
    
    void start(int port);
    void stop();
    
private:
    void setupRoutes();
    
    std::string static_dir_;
    std::shared_ptr<http::HttpServer> http_server_;
    std::shared_ptr<UserManager> user_manager_;
    std::shared_ptr<chat::ChatManager> chat_manager_;

    // Request handlers
    http::HttpResponse handleRegister(const http::HttpRequest& request);
    http::HttpResponse handleLogin(const http::HttpRequest& request);
    http::HttpResponse handleCreateRoom(const http::HttpRequest& request);
    http::HttpResponse handleGetRooms(const http::HttpRequest& request);
    http::HttpResponse handleJoinRoom(const http::HttpRequest& request);
    http::HttpResponse handleSendMessage(const http::HttpRequest& request);
    http::HttpResponse handleGetMessages(const http::HttpRequest& request);
};
