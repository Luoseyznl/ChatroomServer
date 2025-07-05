#pragma once
#include <string>
#include <unordered_map>

/**
 * POST /login?username=alice&id=1 HTTP/1.1\r\n
 * Host: www.example.com\r\n
 * User-Agent: curl/7.64.1\r\n
 * Content-Type: application/x-www-form-urlencoded\r\n
 * Content-Length: 27\r\n
 * \r\n
 * username=alice&password=1234
 */

namespace http {
class HttpRequest {
 public:
  static HttpRequest parse(const std::string& request_str);

  const std::string& method() const { return method_; }
  const std::string& path() const { return path_; }
  const std::string& body() const { return body_; }
  const std::unordered_map<std::string, std::string>& headers() const {
    return headers_;
  }
  const std::unordered_map<std::string, std::string>& query_params() const {
    return query_params_;
  }

 private:
  HttpRequest() = default;
  static std::unordered_map<std::string, std::string> parseQueryParams(
      const std::string& query_string);
  static std::string urlDecode(const std::string& encoded);

  std::string method_;
  std::string path_;
  std::string body_;
  std::unordered_map<std::string, std::string> headers_;
  std::unordered_map<std::string, std::string> query_params_;
};
}  // namespace http