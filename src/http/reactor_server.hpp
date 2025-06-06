#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "event_loop.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

namespace http {

/**
 * @brief 基于Reactor模式的HTTP服务器
 *
 * 使用epoll实现高性能I/O多路复用，采用事件驱动模型
 * 相比传统的线程池模型，能够处理更多的并发连接
 */
class ReactorServer {
 public:
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit ReactorServer(int port);
  ~ReactorServer();

  /**
   * @brief 添加请求处理函数
   * @param path 请求路径，如"/api/users"
   * @param method HTTP方法，如"GET"、"POST"
   * @param handler 处理该路径和方法的函数
   */
  void addHandler(const std::string& path, const std::string& method,
                  RequestHandler handler);

  void setStaticDir(const std::string& dir) { static_dir_ = dir; }
  void setConnectionTimeout(int seconds) { connection_timeout_ = seconds; }
  void setTimeoutCheckInterval(int seconds);

  void run();
  void stop();

 private:
  void acceptLoop();
  HttpResponse handleRequest(const HttpRequest& request);
  void serveStaticFile(const std::string& path, HttpResponse& response);

  int server_fd_;              // 服务器套接字
  int port_;                   // 监听端口
  std::atomic<bool> running_;  // 服务器是否运行中
  std::string static_dir_;     // 静态文件目录
  int connection_timeout_;     // 连接超时时间（秒）

  std::unique_ptr<EventLoop> event_loop_;
  std::thread accept_thread_;

  std::unordered_map<std::string,
                     std::unordered_map<std::string, RequestHandler>>
      handlers_;
};

}  // namespace http