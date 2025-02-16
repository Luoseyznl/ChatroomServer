#include "logger.hpp"

Logger::LogStream Logger::debug(const char* file, const char* function, int line) {
    return LogStream(LogLevel::DEBUG, file, function, line);
}

Logger::LogStream Logger::info(const char* file, const char* function, int line) {
    return LogStream(LogLevel::INFO, file, function, line);
}

Logger::LogStream Logger::warn(const char* file, const char* function, int line) {
    return LogStream(LogLevel::WARN, file, function, line);
}

Logger::LogStream Logger::error(const char* file, const char* function, int line) {
    return LogStream(LogLevel::ERROR, file, function, line);
}

const char* Logger::LogStream::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}
