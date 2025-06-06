#pragma once

#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

namespace utils {

/**
 * @brief 高性能异步日志系统
 *
 * 设计特点:
 * 1. 生产者-消费者模式
 *    - 多个线程作为生产者生成日志消息
 *    - 单独的后台线程作为消费者处理日志写入
 *
 * 2. 线程局部缓冲
 *    - 使用thread_local存储的std::ostringstream，避免频繁构造开销
 *    - 每个线程拥有独立的日志缓冲区，减少线程间竞争
 *
 * 3. 批量处理
 *    - 使用 queue_mutex_ 和 condition_variable 实现线程安全的日志队列批量写入
 *
 * 4. 日志轮转
 *    - 支持按文件大小自动轮转日志文件，限制日志文件总数，自动清理过期日志
 *
 * 5. 用户友好接口 LogStream
 *    - 流式(<<)操作符接口，使用方便直观
 *    - 通过宏定义简化调用，自动包含文件名、函数名和行号
 */
enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4 };

struct LogConfig {
  std::string log_dir;
  size_t max_file_size;
  size_t max_files;
  bool async_mode;

  LogConfig()
      : log_dir("logs"),
        max_file_size(10 * 1024 * 1024),
        max_files(10),
        async_mode(true) {}
};

class Logger {
 public:
  class LogStream {
   public:
    LogStream(LogLevel level, const char* file, const char* function, int line);
    ~LogStream();

    template <typename T>
    LogStream& operator<<(const T& value) {
      if (level_ >= Logger::getGlobalLevel()) {
        stream_ << value;
      }
      return *this;
    }

   private:
    LogLevel level_;
    std::ostringstream& stream_;
  };

  static void init(const LogConfig& config = LogConfig());
  static void setGlobalLevel(LogLevel level);

  static void log(LogLevel level, const char* file, const char* function,
                  int line, const std::string& message);

 private:
  Logger() = default;
  ~Logger();

  static Logger& getInstance();
  static LogLevel getGlobalLevel();

  void initLogger(const LogConfig& config);
  void writeToFile(const std::string& message);
  void processLogQueue();
  void writeLogToFile(const std::string& message);
  void rotateLogFileIfNeeded();
  void cleanOldLogFiles();
  std::string getNewLogFilePath();
  void openLogFile();

  static LogLevel globalLevel_;
  LogConfig config_;
  std::string current_file_path_;
  std::queue<std::string> log_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::thread write_thread_;
  bool stop_thread_ = false;
  std::ofstream log_file_;
};

}  // namespace utils

// 定义便捷宏
#define LOG_DEBUG                                                          \
  utils::Logger::LogStream(utils::LogLevel::DEBUG, __FILE__, __FUNCTION__, \
                           __LINE__)
#define LOG_INFO                                                          \
  utils::Logger::LogStream(utils::LogLevel::INFO, __FILE__, __FUNCTION__, \
                           __LINE__)
#define LOG_WARN                                                          \
  utils::Logger::LogStream(utils::LogLevel::WARN, __FILE__, __FUNCTION__, \
                           __LINE__)
#define LOG_ERROR                                                          \
  utils::Logger::LogStream(utils::LogLevel::ERROR, __FILE__, __FUNCTION__, \
                           __LINE__)
#define LOG_FATAL                                                          \
  utils::Logger::LogStream(utils::LogLevel::FATAL, __FILE__, __FUNCTION__, \
                           __LINE__)
