#pragma once
#include <string>
#include <unordered_map>

namespace http {

class HttpResponse {
 public:
  HttpResponse(int status_code = 200, const std::string& body = "");

  void setStatus(int code);
  void setHeader(const std::string& key, const std::string& value);
  void setBody(const std::string& body);

  int statusCode() const { return statusCode_; };
  const std::string& body() const { return body_; }
  const std::unordered_map<std::string, std::string>& headers() const {
    return headers_;
  };

  std::string toString() const;

 private:
  int statusCode_;
  std::string body_;
  std::unordered_map<std::string, std::string> headers_;

  static std::string getStatusText(int code);
};
}  // namespace http