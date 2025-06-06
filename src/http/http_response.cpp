#include "http/http_response.hpp"

#include <sstream>

namespace http {

HttpResponse::HttpResponse(int code, const std::string& body)
    : status_code(code), body(body) {
  // 检测JSON格式：1. 空内容 2. 以{开始且以}结束 3. 以[开始且以]结束
  if (body.empty() || (body[0] == '{' && body.back() == '}') ||
      (body[0] == '[' && body.back() == ']')) {
    headers["Content-Type"] = "application/json";
    // 将普通消息转换成 {"message": body}
    if (!body.empty() && body[0] != '{' && body[0] != '[') {
      this->body = "{\"message\":\"" + body + "\"}";
    }
  } else {
    headers["Content-Type"] = "text/plain";  // 默认格式
  }
}

/**
 * @brief 将HTTP响应对象转换为完整的HTTP响应字符串
 *
 * HTTP响应格式示例:
 * HTTP/1.1 200 OK
 * Content-Length: 15
 * Content-Type: application/json
 *
 * {"message":"OK"}
 *
 * @return std::string 格式化后的完整HTTP响应字符串
 */
std::string HttpResponse::toString() const {
  std::stringstream ss;
  ss << "HTTP/1.1 " << status_code << " " << getStatusText(status_code)
     << "\r\n";
  ss << "Content-Length: " << body.length() << "\r\n";

  for (const auto& header : headers) {
    ss << header.first << ": " << header.second << "\r\n";
  }

  ss << "\r\n";
  ss << body;

  return ss.str();
}

std::string HttpResponse::getStatusText(int code) {
  switch (code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 302:
      return "Found";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 409:
      return "Conflict";
    case 500:
      return "Internal Server Error";
    default:
      return "Unknown";
  }
}

}  // namespace http
