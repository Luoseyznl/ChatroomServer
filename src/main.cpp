#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "chatroom_server.hpp"
#include "chatroom_server_epoll.hpp"
#include "utils/logger.hpp"

std::atomic<bool> running{true};
// ========== 换 ==========
// ChatroomServer* global_application = nullptr;
ChatroomServerEpoll* global_application = nullptr;  // epoll

/**
 * @brief 信号处理函数
 * 当收到 SIGINT 或 SIGTERM 时，优雅关闭服务器
 */
void signalHandler(int sig) {
  LOG(INFO) << "Received signal " << sig << ", shutting down...";
  running = false;
  if (global_application) {
    // ========== 换 ==========
    // global_application->stopServer();
    global_application->stop();  // epoll
  }
}

/**
 * @brief 注册信号处理函数
 * 使程序能响应 Ctrl+C (SIGINT) 和 kill (SIGTERM) 信号
 */
void setupSignalHandlers() {
  struct sigaction sa;
  sa.sa_handler = signalHandler;     // 指定信号处理函数
  sigemptyset(&sa.sa_mask);          // 不屏蔽其他信号
  sa.sa_flags = 0;                   // 默认行为
  sigaction(SIGINT, &sa, nullptr);   // 注册 Ctrl+C 信号
  sigaction(SIGTERM, &sa, nullptr);  // 注册 kill 信号
}

int main(int argc, char* argv[]) {
  try {
    utils::Logger::initialize();
    LOG(INFO) << "ChatroomServer starting...";

    // 注册信号处理，保证可以优雅退出
    setupSignalHandlers();

    int port = 8080;
    std::string static_dir_path = "static";
    std::string db_file_path = "chat.db";

    if (argc > 1) port = std::stoi(argv[1]);
    if (argc > 2) static_dir_path = argv[2];
    if (argc > 3) db_file_path = argv[3];

    // ========== 换 ==========
    // ChatroomServer app(static_dir_path, db_file_path);
    ChatroomServerEpoll app(static_dir_path, db_file_path, port);  // epoll
    global_application = &app;

    LOG(INFO) << "Server listening on port " << port;

    // ========== 换 ==========
    // app.startServer(port);
    app.start();  // epoll

    LOG(INFO) << "Server running. Press Ctrl+C to stop.";

    // 主循环，等待信号终止
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (global_application) {
      // ========== 换 ==========
      //   global_application->stopServer();
      global_application->stop();  // epoll
      global_application = nullptr;
    }

    LOG(INFO) << "Server shutdown complete";
  } catch (const std::exception& e) {
    LOG(ERROR) << "Fatal error: " << e.what();
    return 1;
  }
  return 0;
}
