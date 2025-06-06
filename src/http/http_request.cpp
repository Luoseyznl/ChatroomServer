#include "http_request.hpp"

#include <iostream>
#include <sstream>
#include <vector>

#include "../utils/logger.hpp"

namespace http {

HttpRequest HttpRequest::parse(const std::string& request_str) {
  HttpRequest request;
  std::istringstream iss(request_str);
  std::string line;

  // 读取请求头并清理换行符
  std::getline(iss, line);
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  std::istringstream request_line(line);
  request_line >> request.method >> request.path;

  // 解析请求头中的查询参数
  request.parseQueryParams();

  // 读取剩余的请求行（含有元数据）
  while (std::getline(iss, line) && line != "\r" && line != "") {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = line.substr(0, colon_pos);
      std::string value = line.substr(colon_pos + 2);  // Skip ": "
      if (!value.empty() && value.back() == '\r') {
        value.pop_back();
      }
      request.headers[key] = value;
    }
  }

  // 在给出 Content-Length 时读取请求体
  auto content_length_it = request.headers.find("Content-Length");
  if (content_length_it != request.headers.end()) {
    size_t content_length = std::stoul(content_length_it->second);

    // 请求头与请求体之间通常有空行
    while (std::getline(iss, line) && (line == "\r" || line == "")) {
      continue;
    }

    request.body = line;
    if (!request.body.empty() && request.body.back() == '\r') {
      request.body.pop_back();
    }

    // 请求体可能跨多行，继续读取直到达到 Content-Length
    if (request.body.length() < content_length) {
      std::vector<char> remaining_buffer(content_length -
                                         request.body.length());
      iss.read(remaining_buffer.data(), remaining_buffer.size());
      request.body += std::string(remaining_buffer.data(), iss.gcount());
    }

    LOG_DEBUG << "Read body with length " << request.body.length()
              << " (expected " << content_length << ")";
  }

  return request;
}

}  // namespace http
