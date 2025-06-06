#include <unistd.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

#include "chat_application.hpp"
#include "utils/logger.hpp"

// 全局变量，用于信号处理
std::atomic<bool> running{true};
ChatApplication* g_app = nullptr;

/**
 * @brief 信号处理函数
 * 捕获 SIGINT (Ctrl+C) 和 SIGTERM 信号，优雅地关闭服务器
 *
 * @param sig 信号编号
 */
void signalHandler(int sig) {
  LOG_INFO << "Received signal " << sig << ", shutting down...";
  running = false;

  // 如果应用实例存在，调用其停止方法
  if (g_app) {
    g_app->stop();
  }
}

/**
 * @brief 注册信号处理函数
 */
void setupSignalHandlers() {
  struct sigaction sa;
  sa.sa_handler = signalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, nullptr);   // Ctrl+C
  sigaction(SIGTERM, &sa, nullptr);  // kill
}

int main(int argc, char* argv[]) {
  try {
    // 初始化日志系统
    utils::Logger::init();
    LOG_INFO << "Chat server starting...";

    // 设置信号处理
    setupSignalHandlers();

    // 解析命令行参数（如果有的话）
    int port = 8080;
    std::string static_dir = "static";

    if (argc > 1) {
      port = std::stoi(argv[1]);
    }

    if (argc > 2) {
      static_dir = argv[2];
    }

    // 创建并启动应用
    ChatApplication app(static_dir);
    g_app = &app;

    LOG_INFO << "Server listening on port " << port;
    app.start(port);

    LOG_INFO << "Server running. Press Ctrl+C to stop.";

    // 主循环：保持程序运行直到收到终止信号
    while (running) {
      // 每秒检查一次运行状态
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 确保应用正确停止（通常在信号处理程序中已经调用）
    if (g_app) {
      g_app->stop();
      g_app = nullptr;
    }

    LOG_INFO << "Server shutdown complete";

  } catch (const std::exception& e) {
    LOG_ERROR << "Fatal error: " << e.what();
    return 1;
  }

  return 0;
}