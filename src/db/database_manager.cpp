#include "database_manager.hpp"

#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>

DatabaseManager::DatabaseManager(const std::string& dbPath)
    : dbPath_(dbPath), db_(nullptr) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
    std::cerr << "Failed to open database: " << sqlite3_errmsg(db_)
              << std::endl;
    db_ = nullptr;
    return;
  }
  if (!initializeDatabase()) {
    std::cerr << "Failed to initialize database tables." << std::endl;
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

DatabaseManager::~DatabaseManager() {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  if (db_) sqlite3_close(db_);
}

bool DatabaseManager::initializeDatabase() {
  const char* userTable =
      "CREATE TABLE IF NOT EXISTS users ("
      "username TEXT PRIMARY KEY,"
      "password TEXT NOT NULL,"
      "is_online INTEGER DEFAULT 0,"
      "last_active_time INTEGER DEFAULT 0);";
  const char* roomTable =
      "CREATE TABLE IF NOT EXISTS rooms ("
      "name TEXT PRIMARY KEY,"
      "creator TEXT NOT NULL,"
      "FOREIGN KEY(creator) REFERENCES users(username));";
  const char* roomUserTable =
      "CREATE TABLE IF NOT EXISTS room_users ("
      "room_name TEXT,"
      "username TEXT,"
      "PRIMARY KEY(room_name, username),"
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"
      "FOREIGN KEY(username) REFERENCES users(username));";
  const char* messageTable =
      "CREATE TABLE IF NOT EXISTS messages ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "room_name TEXT,"
      "username TEXT,"
      "message TEXT,"
      "timestamp INTEGER,"
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"
      "FOREIGN KEY(username) REFERENCES users(username));";
  return executeQuery(userTable) && executeQuery(roomTable) &&
         executeQuery(roomUserTable) && executeQuery(messageTable);
}

bool DatabaseManager::executeQuery(const std::string& query) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << (errMsg ? errMsg : "") << std::endl;
    if (errMsg) sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool DatabaseManager::createUser(const std::string& userName,
                                 const std::string& pwHash) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "INSERT INTO users (username, password) VALUES ('" << userName << "', '"
     << pwHash << "');";
  return executeQuery(ss.str());
}

bool DatabaseManager::validateUser(const std::string& userName,
                                   const std::string& pwHash) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM users WHERE username='" << userName
     << "' AND password='" << pwHash << "';";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
  bool valid = false;
  if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
    valid = sqlite3_column_int(stmt, 0) > 0;
  }
  if (stmt) sqlite3_finalize(stmt);
  return valid;
}

bool DatabaseManager::setUserOnlineStatus(const std::string& userName,
                                          bool onlineStatus) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "UPDATE users SET is_online=" << (onlineStatus ? 1 : 0)
     << " WHERE username='" << userName << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::setUserLastActiveTime(const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  int64_t now = std::chrono::system_clock::now().time_since_epoch().count();
  std::stringstream ss;
  ss << "UPDATE users SET last_active_time=" << now << " WHERE username='"
     << userName << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::isUserOnline(const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "SELECT is_online FROM users WHERE username='" << userName << "';";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
  bool online = false;
  if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
    online = sqlite3_column_int(stmt, 0) > 0;
  }
  if (stmt) sqlite3_finalize(stmt);
  return online;
}

bool DatabaseManager::isUserExists(const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM users WHERE username='" << userName << "';";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
  bool exists = false;
  if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
    exists = sqlite3_column_int(stmt, 0) > 0;
  }
  if (stmt) sqlite3_finalize(stmt);
  return exists;
}

bool DatabaseManager::checkAndUpdateInactiveUsers(const std::string& userName) {
  // 这里简单实现为：如果用户在线，更新时间，否则不处理
  if (isUserOnline(userName)) {
    return setUserLastActiveTime(userName);
  }
  return false;
}

std::vector<User> DatabaseManager::getOnlineUsers() {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<User> users;
  const char* query =
      "SELECT username, password, is_online FROM users WHERE is_online=1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    return users;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string username =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    std::string password =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    bool is_online = sqlite3_column_int(stmt, 2) > 0;
    users.push_back({username, password, is_online});
  }
  if (stmt) sqlite3_finalize(stmt);
  return users;
}

std::vector<User> DatabaseManager::getAllUsers() {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<User> users;
  const char* query = "SELECT username, password, is_online FROM users;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    return users;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string username =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    std::string password =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    bool is_online = sqlite3_column_int(stmt, 2) > 0;
    users.push_back({username, password, is_online});
  }
  if (stmt) sqlite3_finalize(stmt);
  return users;
}

bool DatabaseManager::createRoom(const std::string& roomName,
                                 const std::string& creator) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "INSERT INTO rooms (name, creator) VALUES ('" << roomName << "', '"
     << creator << "');";
  return executeQuery(ss.str());
}

bool DatabaseManager::deleteRoom(const std::string& roomName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "DELETE FROM rooms WHERE name='" << roomName << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::addUserToRoom(const std::string& roomName,
                                    const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "INSERT OR IGNORE INTO room_users (room_name, username) VALUES ('"
     << roomName << "', '" << userName << "');";
  return executeQuery(ss.str());
}

bool DatabaseManager::removeUserFromRoom(const std::string& roomName,
                                         const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "DELETE FROM room_users WHERE room_name='" << roomName
     << "' AND username='" << userName << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::isUserInRoom(const std::string& roomName,
                                   const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM room_users WHERE room_name='" << roomName
     << "' AND username='" << userName << "';";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
  bool exists = false;
  if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
    exists = sqlite3_column_int(stmt, 0) > 0;
  }
  if (stmt) sqlite3_finalize(stmt);
  return exists;
}

bool DatabaseManager::isRoomExists(const std::string& roomName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM rooms WHERE name='" << roomName << "';";
  sqlite3_stmt* stmt = nullptr;
  int rc = sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr);
  bool exists = false;
  if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
    exists = sqlite3_column_int(stmt, 0) > 0;
  }
  if (stmt) sqlite3_finalize(stmt);
  return exists;
}

std::vector<std::string> DatabaseManager::getRoomUsers(
    const std::string& roomName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<std::string> users;
  std::stringstream ss;
  ss << "SELECT username FROM room_users WHERE room_name='" << roomName << "';";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return users;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    users.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  if (stmt) sqlite3_finalize(stmt);
  return users;
}

std::vector<std::string> DatabaseManager::getUserRooms(
    const std::string& userName) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<std::string> rooms;
  std::stringstream ss;
  ss << "SELECT room_name FROM room_users WHERE username='" << userName << "';";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return rooms;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rooms.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  if (stmt) sqlite3_finalize(stmt);
  return rooms;
}

std::vector<std::string> DatabaseManager::getRooms() {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<std::string> rooms;
  const char* query = "SELECT name FROM rooms;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK)
    return rooms;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rooms.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }
  if (stmt) sqlite3_finalize(stmt);
  return rooms;
}

bool DatabaseManager::saveMessage(const std::string& roomName,
                                  const std::string& userName,
                                  const std::string& message,
                                  int64_t timestamp) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::stringstream ss;
  ss << "INSERT INTO messages (room_name, username, message, timestamp) VALUES "
        "('"
     << roomName << "', '" << userName << "', '" << message << "', "
     << timestamp << ");";
  return executeQuery(ss.str());
}

std::vector<nlohmann::json> DatabaseManager::getRoomMessages(
    const std::string& roomName, int64_t since) {
  std::lock_guard<std::recursive_mutex> lock(dbMutex_);
  std::vector<nlohmann::json> messages;
  std::stringstream ss;
  ss << "SELECT username, message, timestamp FROM messages WHERE room_name='"
     << roomName << "'";
  if (since > 0) {
    ss << " AND timestamp > " << since;
  }
  ss << " ORDER BY timestamp ASC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK)
    return messages;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    nlohmann::json msg;
    msg["username"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    msg["content"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    msg["timestamp"] = sqlite3_column_int64(stmt, 2);
    messages.push_back(msg);
  }
  if (stmt) sqlite3_finalize(stmt);
  return messages;
}