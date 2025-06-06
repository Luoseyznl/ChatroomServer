#include "connection.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "http_request.hpp"
#include "http_response.hpp"
#include "utils/logger.hpp"

namespace http {

Connection::Connection(int fd, RequestHandler handler)
    : sockfd_(fd),
      is_closed_(false),
      request_buffer_(),
      response_buffer_(),
      request_handler_(std::move(handler)),
      last_active_time_(time(nullptr)),  // 初始化为当前时间
      timeout_(DEFAULT_TIMEOUT) {
  // 设置为非阻塞模式
  int flags = fcntl(sockfd_, F_GETFL, 0);
  if (flags == -1) {
    LOG_ERROR << "Failed to get socket flags: " << strerror(errno);
  } else {
    if (fcntl(sockfd_, F_SETFL, flags | O_NONBLOCK) == -1) {
      LOG_ERROR << "Failed to set socket non-blocking: " << strerror(errno);
    }
  }
  LOG_DEBUG << "Connection created with fd " << sockfd_
            << ", timeout: " << timeout_ << "s";
}

Connection::~Connection() {
  if (!is_closed_) {
    handleClose();
  }
}

void Connection::handleRead() {
  updateActivityTime();
  readData();
  handleRequest();
}

void Connection::readData() {
  char buffer[4096];
  ssize_t bytes_read;

  while (true) {
    bytes_read = read(sockfd_, buffer, sizeof(buffer));

    if (bytes_read > 0) {
      request_buffer_.append(buffer, bytes_read);
      LOG_DEBUG << "Read " << bytes_read << " bytes from fd " << sockfd_;
    } else if (bytes_read == 0) {
      LOG_INFO << "Client closed connection on fd " << sockfd_;
      handleClose();
      return;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 非阻塞模式下没有更多数据可读
        break;
      } else {
        LOG_ERROR << "Read error on fd " << sockfd_ << ": " << strerror(errno);
        handleClose();
        return;
      }
    }
  }
}

void Connection::handleRequest() {
  if (isClosed() || request_buffer_.empty()) {
    return;
  }

  // 查找HTTP请求结束标记
  if (request_buffer_.find("\r\n\r\n") == std::string::npos) {
    LOG_DEBUG << "Request incomplete, waiting for more data on fd " << sockfd_;
    return;
  }

  try {
    // 解析HTTP请求
    HttpRequest request = HttpRequest::parse(request_buffer_);

    if (request.method.empty() || request.path.empty()) {
      LOG_WARN << "Invalid HTTP request on fd " << sockfd_;
      HttpResponse response(400, "Bad Request");
      handleResponse(response);
      return;
    }

    LOG_INFO << "Processing " << request.method << " request to "
             << request.path << " on fd " << sockfd_;

    request_buffer_.clear();

    if (request_handler_) {
      HttpResponse response = request_handler_(request);
      handleResponse(response);
    } else {
      LOG_ERROR << "No request handler registered for fd " << sockfd_;
      HttpResponse response(500, "Internal Server Error");
      handleResponse(response);
    }
  } catch (const std::exception& e) {
    LOG_ERROR << "Exception parsing HTTP request: " << e.what();
    HttpResponse response(400, "Bad Request");
    handleResponse(response);
  }
}

void Connection::handleResponse(const HttpResponse& response) {
  std::string response_str = response.toString();
  LOG_DEBUG << "Preparing response (" << response_str.size()
            << " bytes) with status " << response.status_code << " on fd "
            << sockfd_;

  response_buffer_ += response_str;

  writeData();
}

void Connection::handleWrite() {
  updateActivityTime();
  writeData();
}

void Connection::writeData() {
  if (isClosed() || response_buffer_.empty()) {
    return;
  }

  while (!response_buffer_.empty()) {
    ssize_t bytes_written =
        write(sockfd_, response_buffer_.data(), response_buffer_.size());

    if (bytes_written > 0) {
      LOG_DEBUG << "Wrote " << bytes_written << " bytes to fd " << sockfd_;
      response_buffer_.erase(0, bytes_written);
    } else if (bytes_written == 0) {
      LOG_WARN << "Zero bytes written on fd " << sockfd_;
      break;
    } else {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // 发送缓冲区已满，稍后再试
        break;
      } else {
        LOG_ERROR << "Write error on fd " << sockfd_ << ": " << strerror(errno);
        handleClose();
        return;
      }
    }
  }
}

void Connection::handleClose() {
  if (is_closed_) {
    return;
  }

  LOG_INFO << "Closing connection with fd " << sockfd_
           << ", idle time: " << getIdleTime() << "s";
  is_closed_ = true;

  // 安全关闭套接字
  if (close(sockfd_) < 0) {
    LOG_ERROR << "Error closing socket " << sockfd_ << ": " << strerror(errno);
  }
}

bool Connection::isTimedOut() const { return getIdleTime() > timeout_; }

time_t Connection::getIdleTime() const {
  return time(nullptr) - last_active_time_;
}

void Connection::updateActivityTime() { last_active_time_ = time(nullptr); }

}  // namespace http