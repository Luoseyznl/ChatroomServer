#include "http_server.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "utils/logger.hpp"

namespace http {

HttpServer::HttpServer(int port)
    : port_(port),
      running_(false),
      thread_pool_(std::thread::hardware_concurrency()),
      static_dir_("") {
  static_dir_ = "./static";

  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::runtime_error("Failed to create socket");
  }

  int opt = 1;  // opt 值为1表示允许地址重用
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("Failed to set socket options");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    throw std::runtime_error("Failed to bind to port");
  }

  if (listen(server_fd_, 10) < 0) {
    throw std::runtime_error("Failed to listen");
  }
}

HttpServer::~HttpServer() {
  stop();
  close(server_fd_);
}

// 添加请求处理函数到路由表
void HttpServer::addHandler(const std::string& path, const std::string& method,
                            RequestHandler handler) {
  handlers_[path][method] = std::move(handler);
}

void HttpServer::run() {
  running_ = true;
  LOG_INFO << "Server listening on port " << port_;

  while (running_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    // 1 接受连接（阻塞）
    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      LOG_ERROR << "Failed to accept connection";
      continue;
    }

    LOG_INFO << "Accepted connection from " << inet_ntoa(client_addr.sin_addr)
             << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd
             << ")";

    // 2 创建一个新线程处理客户端请求
    thread_pool_.enqueue([this, client_fd] {
      handleClient(client_fd);
      return 0;  // For future compatibility
    });
  }
}

void HttpServer::stop() { running_ = false; }

/**
 * @brief 处理单个客户端连接
 *
 * 这个方法：
 * 1. 读取客户端请求
 * 2. 解析HTTP请求
 * 3. 根据路由表找到处理函数
 * 4. 生成HTTP响应
 * 5. 发送响应回客户端
 * 6. 关闭连接
 *
 * @param client_fd 客户端套接字文件描述符
 */
void HttpServer::handleClient(int client_fd) {
  // 1 读取客户端请求
  char buffer[4096];
  ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';
    LOG_DEBUG << "Received raw request:\n" << buffer;

    // 2 解析HTTP请求
    HttpRequest request = HttpRequest::parse(buffer);
    HttpResponse response;

    LOG_INFO << "Request: " << request.method << " " << request.path
             << " (Content-Length: " << request.headers["Content-Length"]
             << ")";
    LOG_DEBUG << "Request body: " << request.body;

    // 3 查表找到处理函数，或者根据路径提供静态文件
    auto path_it = handlers_.find(request.path);
    if (path_it != handlers_.end()) {
      auto method_it = path_it->second.find(request.method);
      if (method_it != path_it->second.end()) {
        LOG_DEBUG << "Found handler for " << request.method << " "
                  << request.path;
        response = method_it->second(request);
        LOG_DEBUG << "Response: " << response.toString();
      } else {
        response = HttpResponse(
            405, "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
        LOG_WARN << "Method not allowed: " << request.method << " "
                 << request.path;
      }

      LOG_DEBUG << "Sending response:\n" << response.toString();

    } else if (request.path == "/" ||
               request.path.find('.') != std::string::npos) {
      // Serve static files
      std::string path = request.path == "/"
                             ? "/index.html"
                             : request.path;  // 默认首页为 index.html
      std::string full_path = static_dir_ + path;
      LOG_DEBUG << "Serving static file: " << full_path;
      serveStaticFile(full_path, response);
    } else {
      LOG_WARN << "Not found: " << request.path;
      response =
          HttpResponse(404, "{\"status\":\"error\",\"message\":\"Not found\"}");
    }

    // Add CORS headers
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type";

    std::string response_str = response.toString();

    ssize_t total_bytes_written = 0;
    const char* data = response_str.c_str();
    size_t remaining = response_str.length();

    // 循环发送响应数据
    while (remaining > 0) {
      ssize_t bytes_written =
          write(client_fd, data + total_bytes_written, remaining);
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

void HttpServer::serveStaticFile(const std::string& path,
                                 HttpResponse& response) {
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
      {"webp", "image/webp"}};

  auto mimeType = mimeTypes.find(ext);
  if (mimeType != mimeTypes.end()) {
    response.headers["Content-Type"] = mimeType->second;
  } else {
    // 默认为二进制流
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
  response.headers["Cache-Control"] =
      "public, max-age=86400";  // Cache for 24 hours

  // Add security headers
  response.headers["X-Content-Type-Options"] = "nosniff";
  response.headers["X-Frame-Options"] = "SAMEORIGIN";
  response.headers["X-XSS-Protection"] = "1; mode=block";

  // Add CORS headers
  response.headers["Access-Control-Allow-Origin"] = "*";
  response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
  response.headers["Access-Control-Allow-Headers"] = "Content-Type";
}

}  // namespace http
