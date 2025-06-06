#include "logger.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>

namespace utils {

// ANSI escape codes for colors
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

static const char* getFileName(const char* filePath);
static const char* getLevelStr(LogLevel level);
static const char* getLevelColor(LogLevel level);

static constexpr size_t BATCH_PROCESSING_THRESHOLD = 100;  // 日志批量处理阈值
LogLevel Logger::globalLevel_ = LogLevel::DEBUG;

void Logger::init(const LogConfig& config) { getInstance().initLogger(config); }

Logger& Logger::getInstance() {
  static Logger instance;
  return instance;
}

void Logger::log(LogLevel level, const char* file, const char* function,
                 int line, const std::string& message) {
  LogStream(level, file, function, line) << message;
}

void Logger::setGlobalLevel(LogLevel level) { globalLevel_ = level; }

LogLevel Logger::getGlobalLevel() { return globalLevel_; }

// 避免高频率构造std::ostringstream， 相当于简易的对象池
static std::ostringstream& getStream() {
  static thread_local std::ostringstream stream;
  stream.str("");  // 清空流内容
  stream.clear();  // 清空错误状态
  return stream;
}

static inline char* printCurrentTime() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm now_tm = {};
#ifdef _WIN32
  if (localtime_s(&now_tm, &now_time) == 0) {
#else
  if (localtime_r(&now_time, &now_tm)) {
#endif  // _WIN32
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now)
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

Logger::LogStream::LogStream(LogLevel level, const char* file,
                             const char* function, int line)
    : level_(level), stream_(getStream()) {
  if (level_ >= Logger::getGlobalLevel()) {
    stream_ << getLevelColor(level_) << '[' << printCurrentTime() << "] " << '['
            << getLevelStr(level_) << "] "
            << "[" << std::this_thread::get_id() << "] " << '['
            << getFileName(file) << ':' << function << ":" << line << "] ";
  }
}

Logger::LogStream::~LogStream() {
  if (level_ >= Logger::getGlobalLevel()) {
    stream_ << "\n";
    const std::string& log_message = stream_.str();
    std::cout << log_message;

    if (level_ >= LogLevel::ERROR) {
      std::cerr << log_message;
      std::cerr.flush();
    }

    Logger::getInstance().writeToFile(log_message);
  }
}

const char* getFileName(const char* filePath) {
  const char* fileName = filePath;
  for (const char* p = filePath; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      fileName = p + 1;
    }
  }
  return fileName;
}

const char* getLevelStr(LogLevel level) {
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

const char* getLevelColor(LogLevel level) {
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

void Logger::initLogger(const LogConfig& config) {
  config_ = config;
  current_file_path_ = getNewLogFilePath();
  openLogFile();
  rotateLogFileIfNeeded();

  if (config_.async_mode) {
    write_thread_ = std::thread(&Logger::processLogQueue, this);
  }
}

void Logger::openLogFile() {
  if (log_file_.is_open()) {
    log_file_.close();
  }
  log_file_.open(current_file_path_, std::ios::app);
}

void Logger::writeLogToFile(const std::string& message) {
  rotateLogFileIfNeeded();
  if (log_file_.is_open()) {
    log_file_ << message;
  }
}

void Logger::rotateLogFileIfNeeded() {
  if (!std::filesystem::exists(current_file_path_)) {
    openLogFile();
    return;
  }

  if (std::filesystem::file_size(current_file_path_) >= config_.max_file_size) {
    current_file_path_ = getNewLogFilePath();
    openLogFile();
    cleanOldLogFiles();
  }
}

void Logger::writeToFile(const std::string& message) {
  if (config_.async_mode) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    log_queue_.push(std::move(message));
    if (log_queue_.size() >= BATCH_PROCESSING_THRESHOLD) {  // 批量处理阈值
      queue_cv_.notify_one();
    }
  } else {
    writeLogToFile(message);
  }
}

void Logger::processLogQueue() {
  std::vector<std::string> batch;
  batch.reserve(BATCH_PROCESSING_THRESHOLD);  // 预分配内存

  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (!log_queue_.empty()) {
      while (!log_queue_.empty() && batch.size() < BATCH_PROCESSING_THRESHOLD) {
        batch.push_back(std::move(log_queue_.front()));
        log_queue_.pop();
      }
      lock.unlock();

      if (!batch.empty()) {
        std::string buffer;
        size_t total_size = 0;
        for (const auto& msg : batch) {
          total_size += msg.size();
        }
        buffer.reserve(total_size);
        for (const auto& msg : batch) {
          buffer.append(msg);
        }
        writeLogToFile(buffer);
        batch.clear();
      }
    } else {
      queue_cv_.wait(lock,
                     [this] { return !log_queue_.empty() || stop_thread_; });
      if (stop_thread_ && log_queue_.empty()) {
        break;
      }
    }
  }
}

void Logger::cleanOldLogFiles() {
  std::vector<std::filesystem::path> log_files;
  for (const auto& entry :
       std::filesystem::directory_iterator(config_.log_dir)) {
    if (entry.path().extension() == ".log") {
      log_files.push_back(entry.path());
    }
  }

  if (log_files.size() > config_.max_files) {
    std::sort(log_files.begin(), log_files.end());
    for (size_t i = 0; i < log_files.size() - config_.max_files; ++i) {
      std::filesystem::remove(log_files[i]);
    }
  }
}

std::string Logger::getNewLogFilePath() {
  std::filesystem::create_directories(config_.log_dir);
  auto now = std::chrono::system_clock::now();
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << config_.log_dir << "/"
     << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S") << ".log";
  return ss.str();
}

Logger::~Logger() {
  if (config_.async_mode) {
    stop_thread_ = true;
    queue_cv_.notify_one();
    if (write_thread_.joinable()) {
      write_thread_.join();
    }
  }
}

}  // namespace utils
