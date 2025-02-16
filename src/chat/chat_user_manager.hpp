#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "user.hpp"

namespace chat {

class ChatUserManager {
public:
    bool registerUser(const std::string& username, const std::string& password);
    bool loginUser(const std::string& username, const std::string& password);
    void logoutUser(const std::string& username);
    bool isUserOnline(const std::string& username) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, User> users_;
};

} // namespace chat
