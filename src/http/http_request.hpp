#pragma once
#include <string>
#include <unordered_map>

namespace http {

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> query_params;

    HttpRequest() = default;
    
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
    void parseParam(const std::string& param) {
        size_t equals_pos = param.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = param.substr(0, equals_pos);
            std::string value = param.substr(equals_pos + 1);
            // URL 解码
            query_params[urlDecode(key)] = urlDecode(value);
        }
    }
    
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

} // namespace http
