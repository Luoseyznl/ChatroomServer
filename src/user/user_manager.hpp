#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

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
    
private:
    std::string getStorageFilePath() const;
    
    std::unordered_map<std::string, std::string> users_; // username -> password
    mutable std::mutex mutex_;
    static constexpr const char* STORAGE_FILE = "users.json";
};
