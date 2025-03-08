#pragma once

#include <string>
#include <sstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <fstream>

namespace utils {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    FATAL = 4
};

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
        
        template<typename T>
        LogStream& operator<<(const T& value) {
            if (level_ >= Logger::getGlobalLevel()) {
                stream_ << value;
            }
            return *this;
        }

    private:
        LogLevel level_;
        const char* file_;
        const char* function_;
        int line_;
        std::ostringstream stream_;
    };

    static void init(const LogConfig& config = LogConfig());
    static void setGlobalLevel(LogLevel level);
    
    static void log(LogLevel level, const char* file, const char* function, int line, const std::string& message);
    
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

} // namespace utils

// 定义便捷宏
#define LOG_DEBUG utils::Logger::LogStream(utils::LogLevel::DEBUG, __FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO  utils::Logger::LogStream(utils::LogLevel::INFO, __FILE__, __FUNCTION__, __LINE__)
#define LOG_WARN  utils::Logger::LogStream(utils::LogLevel::WARN, __FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR utils::Logger::LogStream(utils::LogLevel::ERROR, __FILE__, __FUNCTION__, __LINE__)
#define LOG_FATAL utils::Logger::LogStream(utils::LogLevel::FATAL, __FILE__, __FUNCTION__, __LINE__)
