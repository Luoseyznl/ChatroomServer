#include "reactor_server.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "connection.hpp"
#include "utils/logger.hpp"

namespace http {

/**
 * @brief 创建 server_fd_ 并通过 epoll 监听事件
 */
ReactorServer::ReactorServer(int port)
    : port_(port),
      running_(false),
      static_dir_("./static"),
      connection_timeout_(Connection::DEFAULT_TIMEOUT) {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    LOG_FATAL << "Failed to create socket: " << strerror(errno);
    throw std::runtime_error("Failed to create socket");
  }

  int opt = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    LOG_ERROR << "Failed to set socket options: " << strerror(errno);
    close(server_fd_);
    throw std::runtime_error("Failed to set socket options");
  }

  int flags = fcntl(server_fd_, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR << "Failed to get socket flags: " << strerror(errno);
  } else {
    if (fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
      LOG_ERROR << "Failed to set socket non-blocking: " << strerror(errno);
    }
  }

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
    LOG_FATAL << "Failed to bind to port " << port_ << ": " << strerror(errno);
    close(server_fd_);
    throw std::runtime_error("Failed to bind to port");
  }

  if (listen(server_fd_, SOMAXCONN) < 0) {
    LOG_FATAL << "Failed to listen on port " << port_ << ": "
              << strerror(errno);
    close(server_fd_);
    throw std::runtime_error("Failed to listen on port");
  }

  event_loop_ = std::make_unique<EventLoop>();

  LOG_INFO << "ReactorServer initialized on port " << port_;
}

ReactorServer::~ReactorServer() {
  stop();

  if (server_fd_ >= 0) {
    close(server_fd_);
    LOG_INFO << "Closed server socket";
  }
}

void ReactorServer::setTimeoutCheckInterval(int seconds) {
  if (event_loop_) {
    event_loop_->setTimeoutCheckInterval(seconds);
  }
}

void ReactorServer::addHandler(const std::string& path,
                               const std::string& method,
                               RequestHandler handler) {
  handlers_[path][method] = std::move(handler);
  LOG_INFO << "Added handler for " << method << " " << path;
}

void ReactorServer::run() {
  if (running_) {
    LOG_WARN << "Server is already running";
    return;
  }

  running_ = true;

  // 启动事件循环
  event_loop_->start();

  // 启动接受连接的线程
  accept_thread_ = std::thread(&ReactorServer::acceptLoop, this);

  LOG_INFO << "ReactorServer started on port " << port_;
}

void ReactorServer::stop() {
  if (!running_) {
    return;
  }

  LOG_INFO << "Stopping server...";
  running_ = false;

  // 等待接受线程结束
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  // 停止事件循环
  if (event_loop_) {
    event_loop_->stop();
  }

  LOG_INFO << "Server stopped";
}

void ReactorServer::acceptLoop() {
  LOG_INFO << "Accept loop started";

  while (running_) {
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    // 接受新连接
    int client_fd =
        accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 非阻塞模式下，暂时没有新连接
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else if (errno == EINTR) {
        // 被信号中断，继续尝试
        continue;
      } else {
        LOG_ERROR << "Failed to accept connection: " << strerror(errno);
        continue;
      }
    }

    // 获取客户端IP和端口
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    LOG_INFO << "Accepted connection from " << client_ip << ":" << client_port
             << " (fd: " << client_fd << ")";

    // 创建连接对象
    auto conn = std::make_shared<Connection>(
        client_fd, [this](const HttpRequest& request) -> HttpResponse {
          return this->handleRequest(request);
        });

    // 添加到事件循环
    if (!event_loop_->addConnection(conn)) {
      LOG_ERROR << "Failed to add connection to event loop";
      conn->handleClose();
    }
  }

  LOG_INFO << "Accept loop stopped";
}

HttpResponse ReactorServer::handleRequest(const HttpRequest& request) {
  LOG_INFO << "Handling " << request.method << " request to " << request.path;

  // 查找路由处理函数
  auto path_it = handlers_.find(request.path);
  if (path_it != handlers_.end()) {
    auto method_it = path_it->second.find(request.method);
    if (method_it != path_it->second.end()) {
      // 调用对应的处理函数
      return method_it->second(request);
    } else {
      // 方法不允许
      HttpResponse response(405, "Method Not Allowed");
      response.headers["Allow"] = "GET, POST, OPTIONS";
      return response;
    }
  }

  // 处理静态文件请求
  if (request.path.find("..") == std::string::npos &&  // 防止目录遍历攻击
      (request.path == "/" || request.path.find('.') != std::string::npos)) {
    std::string path = request.path == "/" ? "/index.html" : request.path;
    std::string full_path = static_dir_ + path;

    HttpResponse response;
    serveStaticFile(full_path, response);
    return response;
  }

  // 找不到对应的处理函数，返回404
  return HttpResponse(404, "Not Found");
}

void ReactorServer::serveStaticFile(const std::string& path,
                                    HttpResponse& response) {
  struct stat sb;

  // 检查文件是否存在
  if (stat(path.c_str(), &sb) != 0) {
    response.status_code = 404;
    response.body = "404 Not Found";
    response.headers["Content-Type"] = "text/plain";
    return;
  }

  // 获取文件扩展名
  std::string ext = path.substr(path.find_last_of('.') + 1);

  // 设置适当的Content-Type
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
    response.headers["Content-Type"] = "application/octet-stream";
  }

  // 读取文件内容
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    response.status_code = 500;
    response.body = "500 Internal Server Error";
    response.headers["Content-Type"] = "text/plain";
    return;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  response.status_code = 200;
  response.body = buffer.str();

  // 添加缓存头
  response.headers["Cache-Control"] = "public, max-age=86400";  // 缓存24小时

  // 添加安全头
  response.headers["X-Content-Type-Options"] = "nosniff";
  response.headers["X-Frame-Options"] = "SAMEORIGIN";
  response.headers["X-XSS-Protection"] = "1; mode=block";

  // 添加CORS头
  response.headers["Access-Control-Allow-Origin"] = "*";
  response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
  response.headers["Access-Control-Allow-Headers"] = "Content-Type";
}

}  // namespace http