#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>

#include "../src/utils/logger.hpp"
#include "../src/utils/timer.hpp"

TEST(LoggerTest, LogLevelSetAndGet) {
  using namespace utils;
  Logger::setGlobalLogLevel(LogLevel::INFO);
  EXPECT_EQ(Logger::getGlobalLogLevel(), LogLevel::INFO);
  Logger::setGlobalLogLevel(LogLevel::DEBUG);
  EXPECT_EQ(Logger::getGlobalLogLevel(), LogLevel::DEBUG);
}

TEST(LoggerTest, LogToFile) {
  using namespace utils;
  LogConfig config;
  config.logFilePath = "testlogs/";
  config.asyncLogging = false;
  Logger::initialize(config);

  std::string testMsg = "Test log message";
  LOG(INFO) << testMsg;

  // 等待日志写入
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // 检查日志文件是否生成
  bool found = false;
  for (const auto& entry :
       std::filesystem::directory_iterator(config.logFilePath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".log") {
      std::ifstream fin(entry.path());
      std::string line;
      while (std::getline(fin, line)) {
        if (line.find(testMsg) != std::string::npos) {
          found = true;
          break;
        }
      }
    }
  }
  EXPECT_TRUE(found);
}

TEST(TimerTest, OnceTask) {
  utils::Timer timer;
  std::atomic<bool> called{false};
  timer.start();
  timer.addOnceTask(std::chrono::milliseconds(100), [&] { called = true; });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  timer.stop();
  EXPECT_TRUE(called);
}

TEST(TimerTest, PeriodicTask) {
  utils::Timer timer;
  std::atomic<int> count{0};
  timer.start();
  timer.addPeriodicTask(
      std::chrono::milliseconds(50), std::chrono::milliseconds(50), [&] {
        ++count;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      });
  std::this_thread::sleep_for(std::chrono::milliseconds(220));
  timer.stop();
  EXPECT_GE(count, 3);  // 至少执行3次
}