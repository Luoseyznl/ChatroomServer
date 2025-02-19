#include "http_server.hpp"
#include "../utils/logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

namespace http {

HttpServer::HttpServer(int port)
    : port(port), running(false), thread_pool(std::thread::hardware_concurrency()), static_dir("") {
    
    // Set static directory relative to executable path
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        static_dir = std::string(buffer);
        static_dir = static_dir.substr(0, static_dir.find_last_of('/')) + "/static";
    } else {
        // Fallback to current directory
        static_dir = "../static/";
    }
    
    LOG_INFO << "Static files directory: " << static_dir;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        throw std::runtime_error("Failed to set socket options");
    }
    
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind to port");
    }
    
    if (listen(server_fd, 10) < 0) {
        throw std::runtime_error("Failed to listen");
    }
}

HttpServer::~HttpServer() {
    stop();
    close(server_fd);
}

void HttpServer::addHandler(const std::string& path, const std::string& method, RequestHandler handler) {
    handlers[path][method] = std::move(handler);
}

void HttpServer::run() {
    running = true;
    LOG_INFO << "Server listening on port " << port;
    
    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            LOG_ERROR << "Failed to accept connection";
            continue;
        }
        
        LOG_INFO << "Accepted connection from " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd << ")";
        
        thread_pool.enqueue([this, client_fd] {
            handleClient(client_fd);
            return 0;  // For future compatibility
        });
    }
}

void HttpServer::stop() {
    running = false;
}

void HttpServer::handleClient(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        LOG_DEBUG << "Received raw request:\n" << buffer;
        
        HttpRequest request = HttpRequest::parse(buffer);
        HttpResponse response;
        
        LOG_INFO << "Request: " << request.method << " " << request.path 
                << " (Content-Length: " << request.headers["Content-Length"] << ")";
        LOG_DEBUG << "Request body: " << request.body;
        
        auto path_it = handlers.find(request.path);
        if (path_it != handlers.end()) {
            auto method_it = path_it->second.find(request.method);
            if (method_it != path_it->second.end()) {
                LOG_DEBUG << "Found handler for " << request.method << " " << request.path;
                response = method_it->second(request);
                LOG_DEBUG << "Response: " << response.toString();
            } else {
                response = HttpResponse(405, "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
                LOG_WARN << "Method not allowed: " << request.method << " " << request.path;
            }

            LOG_DEBUG << "Sending response:\n" << response.toString();

        } else if (request.path == "/" || request.path.find('.') != std::string::npos) {
            // Serve static files
            std::string path = request.path == "/" ? "/index.html" : request.path;
            std::string full_path = static_dir + path;
            LOG_DEBUG << "Serving static file: " << full_path;
            serveStaticFile(full_path, response);
        } else {
            LOG_WARN << "Not found: " << request.path;
            response = HttpResponse(404, "{\"status\":\"error\",\"message\":\"Not found\"}");
        }
        
        // Add CORS headers
        response.headers["Access-Control-Allow-Origin"] = "*";
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type";
        
        std::string response_str = response.toString();
        
        ssize_t total_bytes_written = 0;
        const char* data = response_str.c_str();
        size_t remaining = response_str.length();
        
        while (remaining > 0) {
            ssize_t bytes_written = write(client_fd, data + total_bytes_written, remaining);
            if (bytes_written < 0) {
                if (errno == EINTR) {
                    continue;  // 如果被信号中断，重试
                }
                LOG_ERROR << "Failed to send response: " << strerror(errno);
                break;
            }
            total_bytes_written += bytes_written;
            remaining -= bytes_written;
        }
        
        LOG_DEBUG << "Sent " << total_bytes_written << " bytes";
    } else if (bytes_read < 0) {
        LOG_ERROR << "Failed to read from client: " << strerror(errno);
    }
    
    close(client_fd);
}

void HttpServer::serveStaticFile(const std::string& path, HttpResponse& response) {
    struct stat sb;
    if (stat(path.c_str(), &sb) != 0) {
        LOG_ERROR << "File not found: " << path;
        response = HttpResponse(404, "File not found");
        return;
    }
    
    std::string ext = path.substr(path.find_last_of('.') + 1);
    
    // Set appropriate Content-Type header
    const std::unordered_map<std::string, std::string> mimeTypes = {
        {"html", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"txt", "text/plain"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"webp", "image/webp"}
    };
    
    auto mimeType = mimeTypes.find(ext);
    if (mimeType != mimeTypes.end()) {
        response.headers["Content-Type"] = mimeType->second;
    } else {
        response.headers["Content-Type"] = "application/octet-stream";
    }
    
    // Read file content
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        LOG_ERROR << "Failed to open file: " << path;
        response = HttpResponse(500, "Internal Server Error");
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    response.status_code = 200;
    response.body = buffer.str();
    
    // Add caching headers for static files
    response.headers["Cache-Control"] = "public, max-age=86400"; // Cache for 24 hours
    
    // Add security headers
    response.headers["X-Content-Type-Options"] = "nosniff";
    response.headers["X-Frame-Options"] = "SAMEORIGIN";
    response.headers["X-XSS-Protection"] = "1; mode=block";
    
    // Add CORS headers
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type";
}

} // namespace http
