#include "chat_manager.hpp"
#include "../utils/logger.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace chat {

ChatManager::ChatManager(std::shared_ptr<UserManager> user_manager)
    : user_manager_(user_manager) {
    LOG_INFO << "Initializing ChatManager";
}

bool ChatManager::createRoom(const std::string& name, const std::string& creator) {
    LOG_INFO << "Creating room: " << name << " by " << creator;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!user_manager_->isUserRegistered(creator)) {
        LOG_WARN << "User '" << creator << "' is not registered";
        return false;
    }
    
    // Check if room already exists
    if (rooms_.find(name) != rooms_.end()) {
        LOG_WARN << "Room '" << name << "' already exists";
        return false;
    }
    
    // Create new room
    rooms_[name] = std::make_shared<ChatRoom>(name, creator);
    LOG_INFO << "Created room '" << name << "' by user '" << creator << "'";
    return true;
}

bool ChatManager::joinRoom(const std::string& room_name, const std::string& username) {
    LOG_INFO << "Joining room: " << room_name << " for user: " << username;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!user_manager_->isUserRegistered(username)) {
        LOG_WARN << "User '" << username << "' is not registered";
        return false;
    }
    
    auto it = rooms_.find(room_name);
    if (it == rooms_.end()) {
        LOG_WARN << "Room '" << room_name << "' not found";
        return false;
    }
    
    return it->second->addMember(username);
}

std::vector<std::string> ChatManager::getRoomList() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> room_list;
    for (const auto& [name, room] : rooms_) {
        room_list.push_back(name);
    }
    
    return room_list;
}

bool ChatManager::sendMessage(const std::string& room_name, const std::string& sender, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!user_manager_->isUserRegistered(sender)) {
        LOG_WARN << "User '" << sender << "' is not registered";
        return false;
    }
    
    auto it = rooms_.find(room_name);
    if (it == rooms_.end()) {
        LOG_WARN << "Room '" << room_name << "' not found";
        return false;
    }
    
    nlohmann::json message = {
        {"sender", sender},
        {"content", content},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    
    it->second->addMessage(message);
    return true;
}

std::vector<nlohmann::json> ChatManager::getRoomMessages(const std::string& room_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = rooms_.find(room_name);
    if (it == rooms_.end()) {
        return {};
    }
    
    return it->second->getMessages();
}

std::vector<nlohmann::json> ChatManager::getNewMessages(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = rooms_.find(room_name);
    if (it == rooms_.end()) {
        return {};
    }
    
    auto& room = it->second;
    auto& messages = room->getMessages();
    auto& last_position = user_message_positions_[room_name][username];
    
    if (last_position >= messages.size()) {
        return {};
    }
    
    std::vector<nlohmann::json> new_messages(messages.begin() + last_position, messages.end());
    last_position = messages.size();
    
    return new_messages;
}

std::shared_ptr<ChatRoom> ChatManager::findRoom(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(name);
    return it != rooms_.end() ? it->second : nullptr;
}

} // namespace chat
