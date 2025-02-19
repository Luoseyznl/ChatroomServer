#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

class User {
public:
    std::string username;
    std::string password;
};

class UserManager {
public:
    UserManager();
    ~UserManager();
    
    bool registerUser(const std::string& username, const std::string& password);
    bool loginUser(const std::string& username, const std::string& password);
    bool isUserRegistered(const std::string& username) const;
    bool verifyCredentials(const std::string& username, const std::string& password) const;
    bool saveUsers() const;
    void loadUsers();
    bool userExists(const std::string& username) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return users_.find(username) != users_.end();
    }
    
    // 获取所有用户
    std::vector<User> getAllUsers() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<User> all_users;
        all_users.reserve(users_.size());
        for (const auto& [username, password] : users_) {
            all_users.push_back({username, password});
        }
        return all_users;
    }
    
private:
    std::string getStorageFilePath() const;
    
    std::unordered_map<std::string, std::string> users_; // username -> password
    mutable std::mutex mutex_;
    static constexpr const char* STORAGE_FILE = "users.json";
};
