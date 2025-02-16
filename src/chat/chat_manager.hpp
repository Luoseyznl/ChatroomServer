#pragma once

#include "chat_room.hpp"
#include "../user/user_manager.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace chat {

class ChatManager {
public:
    explicit ChatManager(std::shared_ptr<UserManager> user_manager);
    
    bool createRoom(const std::string& name, const std::string& creator);
    bool joinRoom(const std::string& room_name, const std::string& username);
    bool leaveRoom(const std::string& room_name, const std::string& username);
    bool sendMessage(const std::string& room_name, const std::string& sender, const std::string& content);
    
    std::vector<nlohmann::json> getNewMessages(const std::string& room_name, const std::string& username);
    std::vector<nlohmann::json> getRoomMessages(const std::string& room_name) const;
    std::vector<std::string> getRoomList() const;
    std::shared_ptr<ChatRoom> findRoom(const std::string& name) const;
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ChatRoom>> rooms_;
    std::shared_ptr<UserManager> user_manager_;
    std::unordered_map<std::string, std::unordered_map<std::string, size_t>> user_message_positions_;
};

} // namespace chat
