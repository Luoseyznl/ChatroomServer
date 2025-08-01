#include "http_server.hpp"

#include <cstring>
#include <fstream> 
#include <iterator>

#include "http_request.hpp"
#include "http_response.hpp"
#include "socket_compat.hpp"
#include "utils/logger.hpp"

namespace http {
HttpServer::HttpServer(int port, size_t thread_num)
    : port_(port), running_(false), threadPool_(thread_num) {
  staticDir_ = "./static";
  serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd_ < 0) {
    throw std::runtime_error("Failed to create socket");
  }
  int opt = 1;  // opt 值为1表示允许地址重用
  if (setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    throw std::runtime_error("Failed to set socket options");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(serverFd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    throw std::runtime_error("Failed to bind to port");
  }
  if (listen(serverFd_, 10) < 0) {
    throw std::runtime_error("Failed to listen");
  }
}

HttpServer::~HttpServer() {
  stop();
  SOCKET_CLOSE(serverFd_);
}

void HttpServer::addHandler(const std::string& method, const std::string& path,
                            RequestHandler handler) {
  handlers_[method][path] = std::move(handler);
}

void HttpServer::run() {
  running_ = true;
  LOG(INFO) << "HTTP server is running on port " << port_;
  while (running_) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    // 1 接受连接（阻塞）
    int client_fd =
        accept(serverFd_, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      LOG(ERROR) << "Failed to accept connection";
      continue;
    }

    LOG(INFO) << "Accepted connection from " << inet_ntoa(client_addr.sin_addr)
              << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd
              << ")";

    // 2 创建一个新线程处理客户端请求
    threadPool_.enqueue([this, client_fd] {
      handleClient(client_fd);
      return 0;  // For future compatibility
    });
  }
}

void HttpServer::stop() {
  running_ = false;
  LOG(INFO) << "HTTP server is stopping";
}

void HttpServer::handleClient(int client_fd) {
  char buffer[4096];
  ssize_t bytes_read = SOCKET_READ(client_fd, buffer, sizeof(buffer) - 1);

  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';
    LOG(DEBUG) << "Received request:\n" << buffer;

    HttpRequest request = HttpRequest::parse(buffer);
    HttpResponse response;

    std::string content_length;
    auto request_headers = request.headers();
    auto it = request_headers.find("Content-Length");
    if (it != request_headers.end()) {
      content_length = it->second;
    } else {
      content_length = "N/A";
    }
    LOG(INFO) << "Request: " << request.method() << " " << request.path()
              << " (Content-Length: " << content_length << ")";
    LOG(DEBUG) << "Request body: " << request.body();

    auto handler = findHandler(request.method(), request.path());
    if (handler) {
      response = handler(request);
      //   LOG(DEBUG) << "Response: " << response.toString();
    } else {
      response =
          HttpResponse(404, "{\"status\":\"error\",\"message\":\"Not found\"}");
      LOG(WARN) << "Not found: " << request.path();
    }

    // Add CORS headers
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response.setHeader("Access-Control-Allow-Headers", "Content-Type");

    std::string response_str = response.toString();
    ssize_t total_bytes_written = 0;
    const char* data = response_str.c_str();
    size_t remaining = response_str.length();

    // 循环发送响应数据
    while (remaining > 0) {
      ssize_t bytes_written =
          SOCKET_WRITE(client_fd, data + total_bytes_written, remaining);
      if (bytes_written < 0) {
        if (SOCKET_ERROR_MSG() == EINTR) {
          continue;  // 如果被信号中断，重试
        }
        LOG(ERROR) << "Failed to send response: " << strerror(SOCKET_ERROR_MSG());
        break;
      }
      total_bytes_written += bytes_written;
      remaining -= bytes_written;
    }

    LOG(DEBUG) << "Sent " << total_bytes_written << " bytes";
  } else if (bytes_read < 0) {
    LOG(ERROR) << "Failed to read from client: " << strerror(SOCKET_ERROR_MSG());
  }

  SOCKET_CLOSE(client_fd);
}

void HttpServer::sendStaticFile(const std::string& absFilePath, int client_fd) {
  struct stat sb;
  if (stat(absFilePath.c_str(), &sb) != 0) {
    LOG(ERROR) << "File not found: " << absFilePath;
    HttpResponse response(404, "File not found");
    SOCKET_WRITE(client_fd, response.toString().c_str(), response.toString().length());
    return;
  }

  std::ifstream file(absFilePath, std::ios::binary);
  if (!file) {
    LOG(ERROR) << "Failed to open file: " << absFilePath;
    HttpResponse response(500, "Internal Server Error");
    SOCKET_WRITE(client_fd, response.toString().c_str(), response.toString().length());
    return;
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  HttpResponse response(200, content);
  response.setHeader("Content-Length", std::to_string(content.length()));
  SOCKET_WRITE(client_fd, response.toString().c_str(), response.toString().length());
}

HttpServer::RequestHandler HttpServer::findHandler(
    const std::string& method, const std::string& path) const {
  auto methodIt = handlers_.find(method);
  if (methodIt != handlers_.end()) {
    auto pathIt = methodIt->second.find(path);
    if (pathIt != methodIt->second.end()) {
      return pathIt->second;
    }
    // 通配符匹配
    auto wildcardIt = methodIt->second.find("/*");
    if (wildcardIt != methodIt->second.end()) {
      return wildcardIt->second;
    }
  }
  return nullptr;  // 没有找到处理函数
}
}  // namespace http