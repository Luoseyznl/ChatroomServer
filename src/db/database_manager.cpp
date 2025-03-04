#include "database_manager.hpp"
#include "../utils/logger.hpp"
#include <sstream>

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path), db_(nullptr) {
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc) {
            LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db_);
            return;
        }
        LOG_INFO << "Opened database successfully";
    }
    initializeTables();
}

DatabaseManager::~DatabaseManager() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DatabaseManager::initializeTables() {
    // Users table
    const char* create_users_table = 
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password_hash TEXT NOT NULL,"
        "created_at INTEGER NOT NULL);";

    // Rooms table
    const char* create_rooms_table = 
        "CREATE TABLE IF NOT EXISTS rooms ("
        "name TEXT PRIMARY KEY,"
        "creator TEXT NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "FOREIGN KEY(creator) REFERENCES users(username));";

    // Room members table
    const char* create_room_members_table = 
        "CREATE TABLE IF NOT EXISTS room_members ("
        "room_name TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "joined_at INTEGER NOT NULL,"
        "PRIMARY KEY(room_name, username),"
        "FOREIGN KEY(room_name) REFERENCES rooms(name),"
        "FOREIGN KEY(username) REFERENCES users(username));";

    // Messages table
    const char* create_messages_table = 
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "room_name TEXT NOT NULL,"
        "username TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "timestamp INTEGER NOT NULL,"
        "FOREIGN KEY(room_name) REFERENCES rooms(name),"
        "FOREIGN KEY(username) REFERENCES users(username));";

    return executeQuery(create_users_table) &&
           executeQuery(create_rooms_table) &&
           executeQuery(create_room_members_table) &&
           executeQuery(create_messages_table);
}

bool DatabaseManager::executeQuery(const std::string& query) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);
    
    if (rc != SQLITE_OK) {
        LOG_ERROR << "SQL error: " << err_msg;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool DatabaseManager::createUser(const std::string& username, const std::string& password_hash) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "INSERT INTO users (username, password_hash, created_at) VALUES ('"
       << username << "', '" << password_hash << "', "
       << std::chrono::system_clock::now().time_since_epoch().count() << ");";
    return executeQuery(ss.str());
}

bool DatabaseManager::validateUser(const std::string& username, const std::string& password_hash) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "SELECT COUNT(*) FROM users WHERE username = '" << username 
       << "' AND password_hash = '" << password_hash << "';";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    bool valid = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        valid = (sqlite3_column_int(stmt, 0) > 0);
    }
    
    sqlite3_finalize(stmt);
    return valid;
}

bool DatabaseManager::userExists(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "SELECT COUNT(*) FROM users WHERE username = '" << username << "';";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = (sqlite3_column_int(stmt, 0) > 0);
    }
    
    sqlite3_finalize(stmt);
    return exists;
}

bool DatabaseManager::createRoom(const std::string& name, const std::string& creator) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "INSERT INTO rooms (name, creator, created_at) VALUES ('"
       << name << "', '" << creator << "', "
       << std::chrono::system_clock::now().time_since_epoch().count() << ");";
    return executeQuery(ss.str());
}

bool DatabaseManager::deleteRoom(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "DELETE FROM rooms WHERE name = '" << name << "';";
    return executeQuery(ss.str());
}

std::vector<std::string> DatabaseManager::getRooms() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> rooms;
    const char* query = "SELECT name FROM rooms;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        return rooms;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        rooms.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    
    sqlite3_finalize(stmt);
    return rooms;
}

bool DatabaseManager::addRoomMember(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // 首先检查用户是否已经在房间中
    std::stringstream check_ss;
    check_ss << "SELECT COUNT(*) FROM room_members WHERE room_name = '" 
            << room_name << "' AND username = '" << username << "';";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, check_ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    
    bool exists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = (sqlite3_column_int(stmt, 0) > 0);
    }
    
    sqlite3_finalize(stmt);
    
    // 如果用户已经在房间中，直接返回true
    if (exists) {
        return true;
    }
    
    // 用户不在房间中，执行插入操作
    std::stringstream insert_ss;
    insert_ss << "INSERT INTO room_members (room_name, username, joined_at) VALUES ('"
             << room_name << "', '" << username << "', "
             << std::chrono::system_clock::now().time_since_epoch().count() << ");";
    return executeQuery(insert_ss.str());
}

bool DatabaseManager::removeRoomMember(const std::string& room_name, const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "DELETE FROM room_members WHERE room_name = '" << room_name 
       << "' AND username = '" << username << "';";
    return executeQuery(ss.str());
}

std::vector<std::string> DatabaseManager::getRoomMembers(const std::string& room_name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::string> members;
    std::stringstream ss;
    ss << "SELECT username FROM room_members WHERE room_name = '" << room_name << "';";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return members;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        members.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
    }
    
    sqlite3_finalize(stmt);
    return members;
}

bool DatabaseManager::saveMessage(const std::string& room_name, const std::string& username, 
                                const std::string& content, int64_t timestamp) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::stringstream ss;
    ss << "INSERT INTO messages (room_name, username, content, timestamp) VALUES ('"
       << room_name << "', '" << username << "', '" << content << "', " << timestamp << ");";
    return executeQuery(ss.str());
}

std::vector<nlohmann::json> DatabaseManager::getMessages(const std::string& room_name, int64_t since) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<nlohmann::json> messages;
    std::stringstream ss;
    ss << "SELECT username, content, timestamp FROM messages WHERE room_name = '" 
       << room_name << "' ORDER BY timestamp ASC;";
    
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json msg;
        msg["username"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        msg["content"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg["timestamp"] = sqlite3_column_int64(stmt, 2);
        messages.push_back(msg);
    }
    
    sqlite3_finalize(stmt);
    return messages;
}

std::vector<User> DatabaseManager::getAllUsers() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<User> users;
    const char* query = "SELECT username, password_hash FROM users;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR << "Failed to prepare statement: " << sqlite3_errmsg(db_);
        return users;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        users.push_back({std::string(username), std::string(password)});
    }
    
    sqlite3_finalize(stmt);
    return users;
}
