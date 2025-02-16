#include "user_manager.hpp"
#include "../utils/logger.hpp"
#include <fstream>
#include <filesystem>

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
    
    if(users_.empty()) {
        return true;
    }
    std::filesystem::path storage_path = getStorageFilePath();
    LOG_INFO << "Saving user data to: " << storage_path.string();
    try {
        std::filesystem::create_directories(storage_path.parent_path());
        LOG_INFO << "Created user storage directory: " << storage_path.parent_path().string();
        nlohmann::json j;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [username, password] : users_) {
                j[username] = password;
            }
        }

        LOG_INFO << "user size: " << users_.size() << ", json size: " << j.size();
        
        std::ofstream file(storage_path);
        if (!file) {
            LOG_ERROR << "Failed to open user storage file for writing: " << storage_path.string();
            return false;
        }
        
        file << j.dump(4);
        if (file.fail()) {
            LOG_ERROR << "Failed to write user data to file: " << storage_path.string();
            return false;
        }
        
        LOG_INFO << "Saved user data to: " << storage_path.string();
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to save user data: " << e.what();
        return false;
    }
}

void UserManager::loadUsers() {
    std::filesystem::path storage_path = getStorageFilePath();
    
    if (!std::filesystem::exists(storage_path)) {
        LOG_INFO << "User storage file does not exist: " << storage_path.string();
        return;
    }
    
    std::ifstream file(storage_path);
    if (!file) {
        LOG_ERROR << "Failed to open user storage file for reading: " << storage_path.string();
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();
        for (const auto& [username, password] : j.items()) {
            users_[username] = password;
        }
        
        LOG_INFO << "Loaded " << users_.size() << " users from: " << storage_path.string();
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to parse user storage file: " << e.what();
    }
}

std::string UserManager::getStorageFilePath() const {
    return "data/users.json";
}
