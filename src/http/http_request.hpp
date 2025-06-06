#pragma once
#include <string>
#include <unordered_map>

namespace http {

/**
 * @brief HTTP请求类，表示从客户端接收到的HTTP请求
 *
 * HTTP请求：
 * 1. 请求行：HTTP方法(GET/POST等)、请求路径、HTTP版本
 * 2. 请求头：额外的元数据，如Content-Type、Content-Length等
 * 3. 请求体：POST/PUT等请求中包含的数据
 */
class HttpRequest {
 public:
  std::string method;
  std::string path;
  std::string body;
  std::unordered_map<std::string, std::string> headers;
  std::unordered_map<std::string, std::string> query_params;

  HttpRequest() = default;

  // 解析 HTTP 请求字符串
  static HttpRequest parse(const std::string& request_str);

  // 解析 URL 中的查询参数
  void parseQueryParams() {
    size_t pos = path.find('?');
    if (pos != std::string::npos) {
      std::string query_string = path.substr(pos + 1);
      path = path.substr(0, pos);  // 更新 path，移除查询参数部分

      size_t start = 0;
      size_t end;
      while ((end = query_string.find('&', start)) != std::string::npos) {
        parseParam(query_string.substr(start, end - start));
        start = end + 1;
      }
      parseParam(query_string.substr(start));
    }
  }

 private:
  // 辅助函数：解析查询参数中的每个键值对
  void parseParam(const std::string& param) {
    size_t equals_pos = param.find('=');
    if (equals_pos != std::string::npos) {
      std::string key = param.substr(0, equals_pos);
      std::string value = param.substr(equals_pos + 1);
      // URL 解码
      query_params[urlDecode(key)] = urlDecode(value);
    }
  }

  // 辅助函数：URL 解码
  std::string urlDecode(const std::string& encoded) {
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
};

}  // namespace http
