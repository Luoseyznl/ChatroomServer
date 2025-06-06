#pragma once

#include <sqlite3.h>

#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "chat/user.hpp"

class DatabaseManager {
 public:
  DatabaseManager(const std::string& db_path);
  ~DatabaseManager();

  // User operations
  bool createUser(const std::string& username,
                  const std::string& password_hash);
  bool validateUser(const std::string& username,
                    const std::string& password_hash);
  bool userExists(const std::string& username);
  bool setUserOnlineStatus(const std::string& username, bool is_online);
  bool updateUserLastActiveTime(const std::string& username);
  bool checkAndUpdateInactiveUsers(int64_t timeout_ms);
  std::vector<User> getAllUsers();
  // Room operations
  bool createRoom(const std::string& name, const std::string& creator);
  bool deleteRoom(const std::string& name);
  std::vector<std::string> getRooms();

  // Room member operations
  bool addRoomMember(const std::string& room_name, const std::string& username);
  bool removeRoomMember(const std::string& room_name,
                        const std::string& username);
  std::vector<std::string> getRoomMembers(const std::string& room_name);

  // Message operations
  bool saveMessage(const std::string& room_name, const std::string& username,
                   const std::string& content, int64_t timestamp);
  std::vector<nlohmann::json> getMessages(const std::string& room_name,
                                          int64_t since = 0);

 private:
  bool initializeTables();
  bool executeQuery(const std::string& query);

  sqlite3* db_;                 // SQLite数据库连接句柄
  std::string db_path_;         // SQLite数据库文件路径
  std::recursive_mutex mutex_;  // 递归互斥锁（允许同一线程多次锁定）
};
