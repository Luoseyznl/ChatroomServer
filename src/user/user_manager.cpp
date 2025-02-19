#include "user_manager.hpp"
#include "../utils/logger.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <nlohmann/json.hpp>

UserManager::UserManager() {
    loadUsers();
}

UserManager::~UserManager() {
    if (!saveUsers()) {
        LOG_ERROR << "Failed to save user data during shutdown";
    }
}

bool UserManager::registerUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (users_.find(username) != users_.end()) {
        LOG_WARN << "User already exists: " << username;
        return false;
    }
    
    users_[username] = password;
    LOG_INFO << "Registered new user: " << username;
    
    if (!saveUsers()) {
        LOG_ERROR << "Failed to save user data, but user was registered in memory";
    }
    return true;
}

bool UserManager::loginUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        LOG_WARN << "User not found: " << username;
        return false;
    }
    
    if (it->second != password) {
        LOG_WARN << "Invalid password for user: " << username;
        return false;
    }
    
    LOG_INFO << "User logged in: " << username;
    return true;
}

bool UserManager::isUserRegistered(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_.find(username) != users_.end();
}

bool UserManager::verifyCredentials(const std::string& username, const std::string& password) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        return false;
    }
    
    return it->second == password;
}

bool UserManager::saveUsers() const {
    return true;
    std::string storage_path = getStorageFilePath();
    LOG_INFO << "Saving user data to: " << storage_path;
    
    try {
        // 准备用户数据
        nlohmann::json j;
        LOG_INFO << "Preparing user data, total users: " << users_.size();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [username, password] : users_) {
                j[username] = password;
            }
        }
        
        // 如果没有用户，写入空的 JSON 对象
        if (j.empty()) {
            LOG_INFO << "No users to save, writing empty JSON object";
            j = nlohmann::json::object();
        }
        LOG_INFO << "user data:" << j.dump();

        // 使用 POSIX 接口写入文件
        int fd = open(storage_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            LOG_ERROR << "Failed to open file for writing: " << storage_path << ", error: " << strerror(errno);
            return false;
        }
        
        std::string json_str = j.dump(4);
        LOG_INFO << "Writing JSON data, size: " << json_str.size() << " bytes";
        
        ssize_t bytes_written = write(fd, json_str.c_str(), json_str.size());
        if (bytes_written == -1) {
            LOG_ERROR << "Failed to write user data to file: " << storage_path << ", error: " << strerror(errno);
            close(fd);
            return false;
        }
        
        if (static_cast<size_t>(bytes_written) != json_str.size()) {
            LOG_ERROR << "Incomplete write: wrote " << bytes_written << " of " << json_str.size() << " bytes";
            close(fd);
            return false;
        }
        
        // 确保数据写入磁盘
        if (fsync(fd) == -1) {
            LOG_ERROR << "Failed to sync file: " << storage_path << ", error: " << strerror(errno);
            close(fd);
            return false;
        }
        
        close(fd);
        LOG_INFO << "Successfully saved user data to: " << storage_path;
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to save user data: " << e.what();
        return false;
    }
}

void UserManager::loadUsers() {
    std::string storage_path = getStorageFilePath();
    
    struct stat st;
    if (stat(storage_path.c_str(), &st) == -1) {
        if (errno == ENOENT) {
            LOG_INFO << "User storage file does not exist: " << storage_path;
            // 创建一个空的用户文件
            saveUsers();
            return;
        }
        LOG_ERROR << "Failed to check user storage file: " << storage_path << ", error: " << strerror(errno);
        return;
    }
    
    std::ifstream file(storage_path);
    if (!file) {
        LOG_ERROR << "Failed to open user storage file for reading: " << storage_path;
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();
        
        // 确保 JSON 是一个对象
        if (!j.is_object()) {
            LOG_ERROR << "User storage file contains invalid JSON format";
            return;
        }
        
        for (const auto& [username, password] : j.items()) {
            if (password.is_string()) {
                users_[username] = password.get<std::string>();
            } else {
                LOG_WARN << "Invalid password format for user: " << username;
            }
        }
        
        LOG_INFO << "Loaded " << users_.size() << " users from: " << storage_path;
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to parse user storage file: " << e.what();
    }
}

std::string UserManager::getStorageFilePath() const {
    return "./data/users.json";
}
