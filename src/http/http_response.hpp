#pragma once
#include <string>
#include <unordered_map>

namespace http {

class HttpResponse {
public:
    int status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;

    HttpResponse(int code = 200, const std::string& body = "");
    std::string toString() const;

private:
    static std::string getStatusText(int code);
};

} // namespace http
