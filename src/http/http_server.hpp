#pragma once
#include <string>
#include <functional>
#include "utils/thread_pool.hpp"
#include "http/http_request.hpp"
#include "http/http_response.hpp"

namespace http {

class HttpServer {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

    explicit HttpServer(int port);
    ~HttpServer();

    void addHandler(const std::string& path, const std::string& method, RequestHandler handler);
    void run();
    void stop();

private:
    int server_fd;
    int port;
    bool running;
    utils::ThreadPool thread_pool;
    std::unordered_map<std::string, std::unordered_map<std::string, RequestHandler>> handlers;
    std::string static_dir;

    void handleClient(int client_fd);
    void serveStaticFile(const std::string& path, HttpResponse& response);
};

} // namespace http
