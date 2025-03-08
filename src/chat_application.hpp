#pragma once

#include "http/http_server.hpp"
#include "db/database_manager.hpp"
#include <memory>
#include <string>

class ChatApplication {
public:
    ChatApplication(const std::string& static_dir);
    
    void start(int port);
    void stop();
    
private:
    void setupRoutes();
    
    http::HttpResponse serveStaticFile(const std::string& path);

    std::string static_dir_;
    std::unique_ptr<http::HttpServer> http_server_;
    std::shared_ptr<DatabaseManager> db_manager_;
};
