#include "http_request.hpp"

namespace http {
HttpRequest HttpRequest::parse(const std::string& request_str) {
  HttpRequest req;
  // 1. method_
  size_t method_end = request_str.find(' ');
  if (method_end == std::string::npos) return req;  // 无效请求
  req.method_ = request_str.substr(0, method_end);

  size_t path_start = method_end + 1;
  size_t path_end = request_str.find(' ', path_start);
  if (path_end == std::string::npos) return req;  // 无效请求
  std::string full_path = request_str.substr(path_start, path_end - path_start);

  // 2. path_ 和 query_params_
  size_t query_start = full_path.find('?');
  if (query_start != std::string::npos) {
    req.path_ = full_path.substr(0, query_start);
    std::string query_string = full_path.substr(query_start + 1);
    req.query_params_ = parseQueryParams(query_string);
  } else {
    req.path_ = full_path;
  }

  // 3. headers_ 和 body_
  size_t headers_start = request_str.find("\r\n", path_end);
  if (headers_start == std::string::npos) return req;  // 无效
  headers_start += 2;                                  // 跳过 \r\n
  size_t headers_end = request_str.find("\r\n\r\n", headers_start);
  if (headers_end == std::string::npos) return req;  // 无效

  size_t line_start = headers_start;
  while (line_start != std::string::npos && line_start < headers_end) {
    size_t line_end = request_str.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end > headers_end) {
      line_end = headers_end;  // 最后一行可能没有 \r\n
    }
    std::string header_line =
        request_str.substr(line_start, line_end - line_start);

    size_t colon_pos = header_line.find(':');
    if (colon_pos != std::string::npos) {
      std::string key = header_line.substr(0, colon_pos);
      std::string value = header_line.substr(colon_pos + 2);  // 跳过 ": "
      // 去除前后空格
      key.erase(0, key.find_first_not_of(" \t"));
      key.erase(key.find_last_not_of(" \t") + 1);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t") + 1);

      req.headers_[key] = value;
    }

    line_start = (line_end == headers_end) ? std::string::npos
                                           : line_end + 2;  // 跳过 \r\n
  }

  if (headers_end + 4 < request_str.size()) {
    // 读取请求体
    req.body_ = request_str.substr(headers_end + 4);
    if (!req.body_.empty() && req.body_.back() == '\r') {
      req.body_.pop_back();  // 去除可能的 \r
    }
  }
  return req;
}

std::unordered_map<std::string, std::string> HttpRequest::parseQueryParams(
    const std::string& query_string) {
  std::unordered_map<std::string, std::string> params;
  size_t start = 0;
  size_t end;

  while ((end = query_string.find('&', start)) != std::string::npos) {
    std::string param = query_string.substr(start, end - start);
    size_t equals_pos = param.find('=');
    if (equals_pos != std::string::npos) {
      std::string key = urlDecode(param.substr(0, equals_pos));
      std::string value = urlDecode(param.substr(equals_pos + 1));
      params[key] = value;
    }
    start = end + 1;
  }

  // 最后一个参数
  if (start < query_string.size()) {
    std::string param = query_string.substr(start);
    size_t equals_pos = param.find('=');
    if (equals_pos != std::string::npos) {
      std::string key = urlDecode(param.substr(0, equals_pos));
      std::string value = urlDecode(param.substr(equals_pos + 1));
      params[key] = value;
    }
  }

  return params;
}

std::string HttpRequest::urlDecode(const std::string& encoded) {
  std::string decoded;
  for (size_t i = 0; i < encoded.length(); ++i) {
    if (encoded[i] == '%' && i + 2 < encoded.length()) {
      int value;
      std::sscanf(encoded.substr(i + 1, 2).c_str(), "%x", &value);
      decoded += static_cast<char>(value);
      i += 2;
    } else if (encoded[i] == '+') {
      decoded += ' ';
    } else {
      decoded += encoded[i];
    }
  }
  return decoded;
}
}  // namespace http
