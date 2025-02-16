#include "chat_user_manager.hpp"
#include "../utils/logger.hpp"

namespace chat {

bool ChatUserManager::registerUser(const std::string& username, const std::string& password) {
    LOG_INFO << "Attempting to add user: " << username;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (users_.find(username) != users_.end()) {
        LOG_WARN << "User already exists: " << username;
        return false;
    }
    
    users_[username] = User{username, password, false};
    LOG_INFO << "Successfully added user: " << username;
    return true;
}

bool ChatUserManager::loginUser(const std::string& username, const std::string& password) {
    LOG_DEBUG << "Authenticating user: " << username;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = users_.find(username);
    if (it == users_.end() || it->second.password != password) {
        LOG_WARN << "Authentication failed for user: " << username;
        return false;
    }
    
    it->second.is_online = true;
    LOG_INFO << "User authenticated successfully: " << username;
    return true;
}

void ChatUserManager::logoutUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it != users_.end()) {
        it->second.is_online = false;
    }
}

bool ChatUserManager::isUserOnline(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    return it != users_.end() && it->second.is_online;
}

} // namespace chat
