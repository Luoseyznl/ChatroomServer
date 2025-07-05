#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace utils {
struct Color {
  static constexpr const char* RESET = "\033[0m";
  static constexpr const char* RED = "\033[31m";
  static constexpr const char* GREEN = "\033[32m";
  static constexpr const char* YELLOW = "\033[33m";
  static constexpr const char* BLUE = "\033[34m";
  static constexpr const char* MAGENTA = "\033[35m";
  static constexpr const char* CYAN = "\033[36m";
  static constexpr const char* BOLD = "\033[1m";
};

static const char* getFileName(const char* filePath) {
  const char* fileName = filePath;
  for (const char* p = filePath; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      fileName = p + 1;
    }
  }
  return fileName;
}

static const char* getLevelStr(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARN:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  }
}

static const char* getLevelColor(LogLevel level) {
  switch (level) {
    case LogLevel::DEBUG:
      return Color::RESET;
    case LogLevel::INFO:
      return Color::GREEN;
    case LogLevel::WARN:
      return Color::YELLOW;
    case LogLevel::ERROR:
      return Color::RED;
    case LogLevel::FATAL:
      return Color::RED;
    default:
      return Color::RESET;
  }
}

inline static std::ostringstream& getThreadLocalStream() {
  static thread_local std::ostringstream stream;
  stream.str("");  // 清空流内容
  stream.clear();  // 清空错误状态
  return stream;
}

inline static char* getCurrentTime() {
  auto now_timepoint = std::chrono::system_clock::now();  // time_point
  std::time_t now_timestamp =
      std::chrono::system_clock::to_time_t(now_timepoint);
  std::tm now_tm = {};
#ifdef _WIN32
  if (localtime_s(&now_tm, &now_timestamp) == 0) {
#else
  if (localtime_r(&now_timestamp, &now_tm)) {
#endif  // _WIN32
    auto now_ms =
        std::chrono::time_point_cast<std::chrono::milliseconds>(now_timepoint)
            .time_since_epoch()
            .count();
    constexpr int buffer_size = 32;
    static thread_local char buffer[buffer_size];

    auto write = std::snprintf(
        buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
        now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec, (int)(now_ms % 1000));

    buffer[std::min(write, buffer_size - 1)] = 0;
    return buffer;
  } else {
    return (char*)"";
  }
}

static constexpr size_t BATCH_PROCESSING_THRESHOLD = 100;  // 日志批量处理阈值
LogLevel Logger::globalLogLevel_ = LogLevel::DEBUG;

Logger::LogStream::LogStream(LogLevel level, const char* file,
                             const char* function, int line)
    : level_(level), stream_(getThreadLocalStream()) {
  if (level_ >= Logger::getGlobalLogLevel()) {
    stream_ << getLevelColor(level_) << "[" << getLevelStr(level_) << "] "
            << getCurrentTime() << " " << getFileName(file) << ":" << line
            << " " << function << ": " << Color::RESET;
  }
}

Logger::LogStream::~LogStream() {
  if (level_ >= Logger::getGlobalLogLevel()) {
    stream_ << "\n";
    const std::string& log_message = stream_.str();
    std::cout << log_message;

    if (level_ >= LogLevel::ERROR) {
      std::cerr << log_message;
      std::cerr.flush();
    }

    Logger::getInstance().enqueueLogMessage(log_message);
  }
}

void Logger::initialize(const LogConfig& config) {
  getInstance().initLogger(config);
}

void Logger::setGlobalLogLevel(LogLevel level) { globalLogLevel_ = level; }

LogLevel Logger::getGlobalLogLevel() { return globalLogLevel_; }

void Logger::log(LogLevel level, const char* file, const char* function,
                 int line, const std::string& message) {
  LogStream(level, file, function, line) << message;
}

void Logger::initLogger(const LogConfig& config) {
  config_ = config;
  currentFilePath_ = getNewLogFilePath();
  openCurrentLogFile();

  if (config_.asyncLogging) {
    loggingThread_ = std::thread(&Logger::asyncWriteLogToFile, this);
  }
}

void Logger::enqueueLogMessage(const std::string& message) {
  if (config_.asyncLogging) {
    std::lock_guard<std::mutex> lock(logMutex_);
    logQueue_.push(message);
    logCondition_.notify_one();
  } else {
    writeLogToFile(message);
  }
}

void Logger::writeLogToFile(const std::string& message) {
  rotateLogFileIfNeeded();
  if (logFileStream_.is_open()) {
    logFileStream_ << message;
  }
}

void Logger::asyncWriteLogToFile() {
  std::vector<std::string> batch;
  batch.reserve(BATCH_PROCESSING_THRESHOLD);

  while (true) {
    {
      std::unique_lock<std::mutex> lock(logMutex_);
      logCondition_.wait(lock,
                         [this] { return !logQueue_.empty() || stopLogging_; });

      if (stopLogging_ && logQueue_.empty()) {
        break;
      }

      while (!logQueue_.empty() && batch.size() < BATCH_PROCESSING_THRESHOLD) {
        batch.push_back(std::move(logQueue_.front()));
        logQueue_.pop();
      }
    }

    if (!batch.empty()) {
      std::string buffer;
      for (const auto& msg : batch) {
        buffer.append(msg);
      }
      try {
        writeLogToFile(buffer);
      } catch (const std::exception& e) {
        std::cerr << "Logger error: " << e.what() << std::endl;
      }
      batch.clear();
    }
  }
}

void Logger::rotateLogFileIfNeeded() {
  if (!std::filesystem::exists(currentFilePath_)) {
    openCurrentLogFile();
    return;
  }

  if (std::filesystem::file_size(currentFilePath_) >= config_.maxFileSize) {
    currentFilePath_ = getNewLogFilePath();
    openCurrentLogFile();
    purgeExpiredLogFiles();
  }
}

void Logger::purgeExpiredLogFiles() {
  if (config_.maxBackupFiles == 0) {
    return;  // 不需要保留备份文件
  }

  std::vector<std::string> logFiles;
  for (const auto& entry :
       std::filesystem::directory_iterator(config_.logFilePath)) {
    if (entry.is_regular_file() && entry.path().extension() == ".log") {
      logFiles.push_back(entry.path().string());
    }
  }

  std::sort(logFiles.begin(), logFiles.end(),
            [](const std::string& a, const std::string& b) {
              return std::filesystem::last_write_time(a) <
                     std::filesystem::last_write_time(b);
            });

  while (logFiles.size() > config_.maxBackupFiles) {
    std::filesystem::remove(logFiles.front());
    logFiles.erase(logFiles.begin());
  }
}

std::string Logger::getNewLogFilePath() {
  // 确保日志目录存在
  if (!std::filesystem::exists(config_.logFilePath)) {
    std::filesystem::create_directories(config_.logFilePath);
  }

  // 生成带时间戳的日志文件名
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto tm = *std::localtime(&time_t);

  std::ostringstream oss;
  oss << config_.logFilePath << "/chatroom_" << std::setfill('0')
      << std::setw(4) << (tm.tm_year + 1900) << std::setfill('0')
      << std::setw(2) << (tm.tm_mon + 1) << std::setfill('0') << std::setw(2)
      << tm.tm_mday << "_" << std::setfill('0') << std::setw(2) << tm.tm_hour
      << std::setfill('0') << std::setw(2) << tm.tm_min << std::setfill('0')
      << std::setw(2) << tm.tm_sec << ".log";

  return oss.str();
}

void Logger::openCurrentLogFile() {
  if (logFileStream_.is_open()) {
    logFileStream_.close();
  }
  logFileStream_.open(currentFilePath_, std::ios::app);
  if (!logFileStream_.is_open()) {
    std::cerr << "Failed to open log file: " << currentFilePath_ << std::endl;
  }
}

}  // namespace utils
