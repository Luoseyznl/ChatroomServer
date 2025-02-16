#include "http_request.hpp"
#include "../utils/logger.hpp"
#include <sstream>
#include <vector>
#include <iostream>

namespace http {

HttpRequest HttpRequest::parse(const std::string& request_str) {
    HttpRequest request;
    std::istringstream iss(request_str);
    std::string line;

    // Parse request line
    std::getline(iss, line);
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    std::istringstream request_line(line);
    request_line >> request.method >> request.path;

    // 解析查询参数
    request.parseQueryParams();

    // Parse headers
    while (std::getline(iss, line) && line != "\r" && line != "") {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2);
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            request.headers[key] = value;
        }
    }

    // Read body if Content-Length is present
    auto content_length_it = request.headers.find("Content-Length");
    if (content_length_it != request.headers.end()) {
        size_t content_length = std::stoul(content_length_it->second);
        
        // Skip any remaining empty lines
        while (std::getline(iss, line) && (line == "\r" || line == "")) {
            continue;
        }
        
        // The last getline might have consumed part of the body
        request.body = line;
        if (!request.body.empty() && request.body.back() == '\r') {
            request.body.pop_back();
        }
        
        // Read any remaining body content
        if (request.body.length() < content_length) {
            std::vector<char> remaining_buffer(content_length - request.body.length());
            iss.read(remaining_buffer.data(), remaining_buffer.size());
            request.body += std::string(remaining_buffer.data(), iss.gcount());
        }
        
        LOG_DEBUG << "Read body with length " << request.body.length() 
                 << " (expected " << content_length << ")";
    }

    return request;
}

} // namespace http
