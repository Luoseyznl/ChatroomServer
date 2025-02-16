#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>

// ANSI escape codes for colors
struct Color {
    static constexpr const char* RESET   = "\033[0m";
    static constexpr const char* BLACK   = "\033[30m";
    static constexpr const char* RED     = "\033[31m";
    static constexpr const char* GREEN   = "\033[32m";
    static constexpr const char* YELLOW  = "\033[33m";
    static constexpr const char* BLUE    = "\033[34m";
    static constexpr const char* MAGENTA = "\033[35m";
    static constexpr const char* CYAN    = "\033[36m";
    static constexpr const char* WHITE   = "\033[37m";
    static constexpr const char* BOLD    = "\033[1m";
};

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    class LogStream {
    public:
        LogStream(LogLevel level, const char* file, const char* function, int line) 
            : level_(level), file_(getFileName(file)), function_(function), line_(line) {}
        
        ~LogStream() {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            std::stringstream ss;
            ss << Color::CYAN << "["
               << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
               << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
               << getLevelColorString(level_)
               << Color::MAGENTA << "[Thread-" << std::this_thread::get_id() << "] "
               << Color::BLUE << "[" << file_ << ":" << line_ << "] "
               << Color::CYAN << "[" << function_ << "] "
               << getLevelColor(level_) << buffer_.str()
               << Color::RESET << std::endl;

            std::cout << ss.str();
        }

        template<typename T>
        LogStream& operator<<(const T& value) {
            buffer_ << value;
            return *this;
        }

    private:
        LogLevel level_;
        const char* file_;
        const char* function_;
        int line_;
        std::stringstream buffer_;

        static const char* getFileName(const char* filePath) {
            const char* fileName = filePath;
            for (const char* p = filePath; *p; ++p) {
                if (*p == '/' || *p == '\\') {
                    fileName = p + 1;
                }
            }
            return fileName;
        }

        static const char* getLevelColor(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG: return Color::WHITE;
                case LogLevel::INFO:  return Color::GREEN;
                case LogLevel::WARN:  return Color::YELLOW;
                case LogLevel::ERROR: return Color::RED;
                default:              return Color::RESET;
            }
        }

        static std::string getLevelColorString(LogLevel level) {
            const char* color = getLevelColor(level);
            const char* levelStr = getLevelString(level);
            std::stringstream ss;
            ss << color << Color::BOLD << "[" << levelStr << "] ";
            return ss.str();
        }

        static const char* getLevelString(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO ";
                case LogLevel::WARN:  return "WARN ";
                case LogLevel::ERROR: return "ERROR";
                default:              return "UNKNOWN";
            }
        }
    };

    static LogStream debug(const char* file, const char* function, int line) {
        return LogStream(LogLevel::DEBUG, file, function, line);
    }

    static LogStream info(const char* file, const char* function, int line) {
        return LogStream(LogLevel::INFO, file, function, line);
    }

    static LogStream warn(const char* file, const char* function, int line) {
        return LogStream(LogLevel::WARN, file, function, line);
    }

    static LogStream error(const char* file, const char* function, int line) {
        return LogStream(LogLevel::ERROR, file, function, line);
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
};

#define LOG_DEBUG Logger::getInstance().debug(__FILE__, __FUNCTION__, __LINE__)
#define LOG_INFO  Logger::getInstance().info(__FILE__, __FUNCTION__, __LINE__)
#define LOG_WARN  Logger::getInstance().warn(__FILE__, __FUNCTION__, __LINE__)
#define LOG_ERROR Logger::getInstance().error(__FILE__, __FUNCTION__, __LINE__)
