#pragma once

#include <ctime>  // time_t
#include <functional>
#include <memory>
#include <string>

#include "http_request.hpp"
#include "http_response.hpp"

namespace http {

/**
 * @brief 单个客户端连接的封装
 *
 * 负责：
 * 1. 管理连接生命周期
 * 2. 非阻塞I/O
 * 3. HTTP请求解析
 * 4. HTTP响应生成与发送
 */
class Connection : public std::enable_shared_from_this<Connection> {
 public:
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  static constexpr time_t DEFAULT_TIMEOUT = 60;  // 60秒

  explicit Connection(int fd, RequestHandler handler);
  ~Connection();

  // 事件循环中调用
  void handleRead();
  void handleWrite();
  void handleClose();

  // 连接状态
  bool isClosed() const { return is_closed_; }
  bool isTimedOut() const;
  bool hasDataToWrite() const { return !response_buffer_.empty(); }

  // 活动时间
  time_t getIdleTime() const;
  int getFd() const { return sockfd_; }
  void updateActivityTime();

 private:
  void readData();       // 内部：从fd读数据到请求缓冲区
  void writeData();      // 内部：从响应缓冲区写数据到fd
  void handleRequest();  // 内部：请求解析
  void handleResponse(const HttpResponse& response);  // 内部：响应生成

  int sockfd_;
  bool is_closed_;
  std::string request_buffer_;
  std::string response_buffer_;
  RequestHandler request_handler_;
  time_t last_active_time_;
  time_t timeout_;
};

}  // namespace http
