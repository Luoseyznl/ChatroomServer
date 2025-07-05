#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include "http_request.hpp"
#include "http_response.hpp"
#include "utils/thread_pool.hpp"

namespace http {
class HttpServer {
 public:
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit HttpServer(int port, size_t thread_num = 4);
  ~HttpServer();

  void addHandler(const std::string& method, const std::string& path,
                  RequestHandler handler);
  void run();
  void stop();

 private:
  int serverFd_{-1};
  int port_{0};
  std::atomic<bool> running_{false};
  utils::ThreadPool threadPool_;
  std::string staticDir_{"./static"};

  // method -> path -> handler
  std::unordered_map<std::string,
                     std::unordered_map<std::string, RequestHandler>>
      handlers_;

  void handleClient(int clientFd);
  void sendStaticFile(const std::string& absFilePath, int clientFd);
  RequestHandler findHandler(const std::string& method,
                             const std::string& path) const;
};
}  // namespace http
