#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

namespace utils {
enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3, FATAL = 4 };

struct LogConfig {
  std::string logFilePath;
  size_t maxFileSize;
  size_t maxBackupFiles;
  bool asyncLogging;
  LogConfig()
      : logFilePath("logs/"),
        maxFileSize(10 * 1024 * 1024),  // 10 MB
        maxBackupFiles(10),
        asyncLogging(false) {}
};

class Logger {
 public:
  class LogStream {
   public:
    LogStream(LogLevel level, const char* file, const char* function, int line);
    ~LogStream();

    template <typename T>
    LogStream& operator<<(const T& message) {
      if (level_ >= Logger::getGlobalLogLevel()) {
        stream_ << message;
      }
      return *this;
    }

   private:
    LogLevel level_;
    std::ostringstream& stream_;
  };

  static void initialize(const LogConfig& config = LogConfig());
  static void setGlobalLogLevel(LogLevel level);
  static LogLevel getGlobalLogLevel();
  static void log(LogLevel level, const char* file, const char* function,
                  int line, const std::string& message);

 private:
  Logger() = default;
  ~Logger() = default;

  inline static Logger& getInstance() {
    static Logger instance;
    return instance;
  }

  void initLogger(const LogConfig& config);
  void enqueueLogMessage(const std::string& message);
  void writeLogToFile(const std::string& message);
  void asyncWriteLogToFile();
  void rotateLogFileIfNeeded();
  void purgeExpiredLogFiles();
  std::string getNewLogFilePath();
  void openCurrentLogFile();

  static LogLevel globalLogLevel_;
  LogConfig config_;
  std::string currentFilePath_;

  std::thread loggingThread_;
  std::mutex logMutex_;
  std::condition_variable logCondition_;
  std::queue<std::string> logQueue_;

  std::ofstream logFileStream_;
  std::atomic<bool> stopLogging_{false};
};

}  // namespace utils

#define LOG(level)                                                         \
  utils::Logger::LogStream(utils::LogLevel::level, __FILE__, __FUNCTION__, \
                           __LINE__)
