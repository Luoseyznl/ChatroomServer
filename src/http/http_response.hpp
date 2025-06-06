#pragma once
#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief 服务器返回给客户端的HTTP响应类
 *
 * HTTP响应：
 * 1. 状态行：包含HTTP版本、状态码、状态消息
 * 2. 响应头：包含额外的元数据，如Content-Type、Content-Length等
 * 3. 响应体：返回给客户端的实际内容
 */
class HttpResponse {
 public:
  int status_code;
  std::string body;
  std::unordered_map<std::string, std::string> headers;

  HttpResponse(int code = 200, const std::string& body = "");
  std::string toString() const;

 private:
  static std::string getStatusText(int code);
};

}  // namespace http
