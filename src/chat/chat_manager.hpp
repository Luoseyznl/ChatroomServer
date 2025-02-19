#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace chat {

class ChatManager {
public:
    ChatManager() noexcept = default;
    
    bool createRoom(const std::string& name, const std::string& creator);
    bool joinRoom(const std::string& room_name, const std::string& username);
    bool leaveRoom(const std::string& room_name, const std::string& username);
    bool sendMessage(const std::string& room_name, const std::string& sender, const json& message);
    
    std::vector<json> getNewMessages(const std::string& room_name, const std::string& username);
    std::vector<json> getRoomMessages(const std::string& room_name) const;
    std::vector<std::string> getRooms() const;
    std::vector<std::string> getRoomMembers(const std::string& room_name) const;
    bool isUserInRoom(const std::string& room_name, const std::string& username) const;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<json>> rooms_;
    std::unordered_map<std::string, std::unordered_set<std::string>> room_members_;
};

} // namespace chat
