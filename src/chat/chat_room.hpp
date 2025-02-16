#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "nlohmann/json.hpp"

namespace chat {

class ChatRoom {
public:
    ChatRoom() = default;
    ChatRoom(const std::string& name, const std::string& creator);
    
    // Delete copy operations
    ChatRoom(const ChatRoom&) = delete;
    ChatRoom& operator=(const ChatRoom&) = delete;
    
    // Allow move operations
    ChatRoom(ChatRoom&& other) noexcept;
    ChatRoom& operator=(ChatRoom&& other) noexcept;

    void addMessage(const nlohmann::json& message);
    bool addMember(const std::string& username);
    bool isMember(const std::string& username) const;
    bool hasMember(const std::string& username) const;
    
    nlohmann::json toJson() const;
    static void fromJson(ChatRoom& room, const nlohmann::json& j);

    const std::string& getName() const { return name_; }
    const std::string& getCreator() const { return creator_; }
    const std::vector<std::string>& getMembers() const;
    const std::vector<nlohmann::json>& getMessages() const;

private:
    std::string name_;
    std::string creator_;
    std::vector<std::string> members_;
    std::vector<nlohmann::json> messages_;
    mutable std::mutex mutex_;
};

} // namespace chat
