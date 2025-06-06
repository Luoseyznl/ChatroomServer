#pragma once
#include <functional>
#include <string>

#include "http/http_request.hpp"
#include "http/http_response.hpp"
#include "utils/thread_pool.hpp"
#include "utils/work_stealing_thread_pool.hpp"
namespace http {

/**
 * @brief HTTP服务器类，用于处理HTTP请求和响应
 *
 * 提供了基本的HTTP服务器功能：
 * 1. 监听指定端口
 * 2. 接受客户端连接
 * 3. 解析HTTP请求
 * 4. 路由请求到相应的处理函数
 * 5. 发送HTTP响应
 * 6. 提供静态文件服务
 */
class HttpServer {
 public:
  /**
   * @brief 请求处理函数类型
   *
   * HttpRequest -> HttpResponse
   */
  using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

  explicit HttpServer(int port);
  ~HttpServer();

  /**
   * @brief 添加请求处理函数
   *
   * @param path 请求路径，如"/api/users"
   * @param method HTTP方法，如"GET"、"POST"
   * @param handler 处理该路径和方法的函数
   */
  void addHandler(const std::string& path, const std::string& method,
                  RequestHandler handler);
  void run();
  void stop();

 private:
  int server_fd_;
  int port_;
  bool running_;
  utils::WorkStealingThreadPool thread_pool_;  // 使用工作窃取线程池
  // utils::ThreadPool thread_pool_;

  /**
   * 路由表：存储请求路径、HTTP方法和对应的处理函数
   * 第一层map键为路径，值为另一个map
   * 第二层map键为HTTP方法，值为处理函数
   */
  std::unordered_map<std::string,
                     std::unordered_map<std::string, RequestHandler>>
      handlers_;
  std::string static_dir_;

  void handleClient(int client_fd);
  void serveStaticFile(const std::string& path, HttpResponse& response);
};

}  // namespace http
