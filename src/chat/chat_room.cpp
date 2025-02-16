#include "chat_room.hpp"
#include "../utils/logger.hpp"
#include <algorithm>

namespace chat {

ChatRoom::ChatRoom(const std::string& name, const std::string& creator)
    : name_(name), creator_(creator) {
    members_.push_back(creator);
    LOG_INFO << "Created room '" << name_ << "' with creator '" << creator_ << "'";
}

ChatRoom::ChatRoom(ChatRoom&& other) noexcept
    : name_(std::move(other.name_))
    , creator_(std::move(other.creator_))
    , members_(std::move(other.members_))
    , messages_(std::move(other.messages_)) {
    // mutex_ is neither copyable nor movable, but it's default constructed in the new object
}

ChatRoom& ChatRoom::operator=(ChatRoom&& other) noexcept {
    if (this != &other) {
        name_ = std::move(other.name_);
        creator_ = std::move(other.creator_);
        members_ = std::move(other.members_);
        messages_ = std::move(other.messages_);
        // mutex_ remains as is
    }
    return *this;
}

void ChatRoom::addMessage(const nlohmann::json& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back(message);
    LOG_DEBUG << "Added message to room '" << name_ << "'";
}

bool ChatRoom::isMember(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(members_.begin(), members_.end(), username) != members_.end();
}

bool ChatRoom::addMember(const std::string& username) {
    LOG_DEBUG << "ChatRoom::addMember - Attempting to add member '" << username << "' to room '" << name_ << "'";
    
    if (isMember(username)) {
        LOG_WARN << "User '" << username << "' is already a member of room '" << name_ << "'";
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    members_.push_back(username);
    LOG_INFO << "Successfully added member '" << username << "' to room '" << name_ << "'";
    return true;
}

bool ChatRoom::hasMember(const std::string& username) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(members_.begin(), members_.end(), username) != members_.end();
}

const std::vector<std::string>& ChatRoom::getMembers() const {
    return members_;
}

const std::vector<nlohmann::json>& ChatRoom::getMessages() const {
    return messages_;
}

nlohmann::json ChatRoom::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["name"] = name_;
    j["creator"] = creator_;
    j["members"] = members_;
    j["messages"] = messages_;
    return j;
}

void ChatRoom::fromJson(ChatRoom& room, const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(room.mutex_);
    room.name_ = j["name"].get<std::string>();
    room.creator_ = j["creator"].get<std::string>();
    room.members_ = j["members"].get<std::vector<std::string>>();
    room.messages_ = j["messages"].get<std::vector<nlohmann::json>>();
}

} // namespace chat
