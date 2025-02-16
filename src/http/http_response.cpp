#include "http/http_response.hpp"
#include <sstream>

namespace http {

HttpResponse::HttpResponse(int code, const std::string& body)
    : status_code(code), body(body) {
    // 如果消息体是 JSON 格式的，设置 Content-Type 为 application/json
    if (body.empty() || (body[0] == '{' && body.back() == '}') || 
        (body[0] == '[' && body.back() == ']')) {
        headers["Content-Type"] = "application/json";
        // 如果只是一个普通字符串消息，将其转换为 JSON 格式
        if (!body.empty() && body[0] != '{' && body[0] != '[') {
            this->body = "{\"message\":\"" + body + "\"}";
        }
    } else {
        headers["Content-Type"] = "text/plain";
    }
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    ss << "HTTP/1.1 " << status_code << " " << getStatusText(status_code) << "\r\n";
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
        case 200: return "OK";
        case 201: return "Created";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 409: return "Conflict";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

}
