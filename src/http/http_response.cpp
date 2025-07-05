#include "http_response.hpp"

#include <sstream>

namespace http {

void HttpResponse::setStatus(int code) { statusCode_ = code; }

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
  headers_[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
  body_ = body;
  headers_["Content-Length"] = std::to_string(body.length());
  if (headers_.find("Content-Type") == headers_.end()) {
    if (!body.empty() && ((body.front() == '{' && body.back() == '}') ||
                          (body.front() == '[' && body.back() == ']'))) {
      headers_["Content-Type"] = "application/json";
    } else {
      headers_["Content-Type"] = "text/plain";
    }
  }
}

HttpResponse::HttpResponse(int status_code, const std::string& body)
    : statusCode_(status_code) {
  setBody(body);
}

std::string HttpResponse::toString() const {
  std::stringstream ss;
  ss << "HTTP/1.1 " << statusCode_ << " " << getStatusText(statusCode_)
     << "\r\n";
  ss << "Content-Length: " << body_.length() << "\r\n";
  for (const auto& header : headers_) {
    if (header.first == "Content-Length") continue;
    ss << header.first << ": " << header.second << "\r\n";
  }
  ss << "\r\n";
  ss << body_;
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
    case 500:
      return "Internal Server Error";
    default:
      return "Unknown Status";
  }
}

}  // namespace http