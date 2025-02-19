#include "chat_manager.hpp"
#include "../utils/logger.hpp"
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace chat {

bool ChatManager::createRoom(const std::string& name, const std::string& creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rooms_.find(name) != rooms_.end()) {
        return false;
    }
    
    rooms_[name] = std::vector<nlohmann::json>();
    room_members_[name] = std::unordered_set<std::string>{creator};
    return true;
}

bool ChatManager::joinRoom(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_name);
    if (it == room_members_.end()) {
        return false;
    }
    
    it->second.insert(username);
    return true;
}

bool ChatManager::leaveRoom(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = room_members_.find(room_name);
    if (it == room_members_.end()) {
        return false;
    }
    
    it->second.erase(username);
    return true;
}

bool ChatManager::sendMessage(const std::string& room_name, const std::string& username, const nlohmann::json& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto room_it = rooms_.find(room_name);
    if (room_it == rooms_.end()) {
        LOG_ERROR << "Room not found: " << room_name;
        return false;
    }
    
    // 检查发送者是否在房间中
    auto members_it = room_members_.find(room_name);
    if (members_it == room_members_.end() || members_it->second.find(username) == members_it->second.end()) {
        LOG_ERROR << "Sender not in room: " << username << " in " << room_name;
        return false;
    }
    
    // 使用服务器时间作为时间戳
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    nlohmann::json message = {
        {"room", room_name},
        {"username", username},
        {"content", content},
        {"timestamp", timestamp}
    };
    
    room_it->second.push_back(message);
    LOG_INFO << "Message sent in room " << room_name << " by " << username;
    return true;
}

std::vector<nlohmann::json> ChatManager::getNewMessages(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto room_it = rooms_.find(room_name);
    if (room_it == rooms_.end()) {
        LOG_ERROR << "Room not found: " << room_name;
        return {};
    }
    
    // 返回所有消息，后续可以添加消息 ID 或时间戳来只返回新消息
    return room_it->second;
}

std::vector<nlohmann::json> ChatManager::getRoomMessages(const std::string& room_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = rooms_.find(room_name);
    if (it == rooms_.end()) {
        return std::vector<nlohmann::json>();
    }
    
    return it->second;
}

std::vector<std::string> ChatManager::getRooms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> room_names;
    room_names.reserve(rooms_.size());
    
    for (const auto& [name, _] : rooms_) {
        room_names.push_back(name);
    }
    
    return room_names;
}

std::vector<std::string> ChatManager::getRoomMembers(const std::string& room_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = room_members_.find(room_name);
    if (it == room_members_.end()) {
        return std::vector<std::string>();
    }
    
    return std::vector<std::string>(it->second.begin(), it->second.end());
}

bool ChatManager::isUserInRoom(const std::string& room_name, const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = room_members_.find(room_name);
    if (it == room_members_.end()) {
        return false;
    }
    
    return it->second.find(username) != it->second.end();
}

} // namespace chat
