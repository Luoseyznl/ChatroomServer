#pragma once

#include <string>
#include <memory>
#include <vector>
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <mutex>
#include "../user/user_manager.hpp"

class DatabaseManager {
public:
    DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // User operations
    bool createUser(const std::string& username, const std::string& password_hash);
    bool validateUser(const std::string& username, const std::string& password_hash);
    bool userExists(const std::string& username);
    std::vector<User> getAllUsers();
    
    // Room operations
    bool createRoom(const std::string& name, const std::string& creator);
    bool deleteRoom(const std::string& name);
    std::vector<std::string> getRooms();
    
    // Room member operations
    bool addRoomMember(const std::string& room_name, const std::string& username);
    bool removeRoomMember(const std::string& room_name, const std::string& username);
    std::vector<std::string> getRoomMembers(const std::string& room_name);
    
    // Message operations
    bool saveMessage(const std::string& room_name, const std::string& username, 
                    const std::string& content, int64_t timestamp);
    std::vector<nlohmann::json> getMessages(const std::string& room_name, int64_t since = 0);

private:
    bool initializeTables();
    bool executeQuery(const std::string& query);
    
    sqlite3* db_;
    std::string db_path_;
    std::recursive_mutex mutex_;
};
