#include <unistd.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "chatroom_server.hpp"
#include "chatroom_server_epoll.hpp"
#include "http/socket_compat.hpp"
#include "utils/logger.hpp"

std::atomic<bool> running{true};
// ========== 换 ==========
ChatroomServer* global_application = nullptr;
// ChatroomServerEpoll* global_application = nullptr;  // epoll

/**
 * @brief 信号处理函数
 * 当收到 SIGINT 或 SIGTERM 时，优雅关闭服务器
 */
void signalHandler(int sig) {
  LOG(INFO) << "Received signal " << sig << ", shutting down...";
  running = false;
  if (global_application) {
    global_application->stopServer();
  }
}

/**
 * @brief 注册信号处理函数
 * 使程序能响应 Ctrl+C (SIGINT) 和 kill (SIGTERM) 信号
 */
void setupSignalHandlers() {
#ifdef _WIN32
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);
#else
  struct sigaction sa;
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
#endif
}

int main(int argc, char* argv[]) {
  if (!initSockets()) {
    std::cerr << "Failed to initialize sockets" << std::endl;
    return 1;
  }

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

    // ============== 换 ==============
    ChatroomServer app(static_dir_path, db_file_path, port, "localhost:9092");
    // ChatroomServerEpoll app(static_dir_path, db_file_path, port,
    //                         "localhost:9092"); // epoll
    global_application = &app;

    LOG(INFO) << "Server listening on port " << port;

    app.startServer();

    LOG(INFO) << "Server running. Press Ctrl+C to stop.";

    // 主循环，等待信号终止
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (global_application) {
      global_application->stopServer();
      global_application = nullptr;
    }

    LOG(INFO) << "Server shutdown complete";
  } catch (const std::exception& e) {
    LOG(ERROR) << "Fatal error: " << e.what();
    return 1;
  }

  cleanupSockets();
  return 0;
}
