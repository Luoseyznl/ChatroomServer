#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "chat/user.hpp"

class DatabaseManager {
 public:
  DatabaseManager(const std::string& dbPath);
  ~DatabaseManager();

  bool createUser(const std::string& userName, const std::string& pwHash);
  bool validateUser(const std::string& userName, const std::string& pwHash);
  bool setUserOnlineStatus(const std::string& userName, bool onlineStatus);
  bool setUserLastActiveTime(const std::string& userName);
  bool isUserOnline(const std::string& userName);
  bool isUserExists(const std::string& userName);
  bool checkAndUpdateInactiveUsers(const std::string& userName);
  std::vector<User> getOnlineUsers();
  std::vector<User> getAllUsers();

  bool createRoom(const std::string& roomName, const std::string& creator);
  bool deleteRoom(const std::string& roomName);
  bool addUserToRoom(const std::string& roomName, const std::string& userName);
  bool removeUserFromRoom(const std::string& roomName,
                          const std::string& userName);
  bool isUserInRoom(const std::string& roomName, const std::string& userName);
  bool isRoomExists(const std::string& roomName);
  std::vector<std::string> getRoomUsers(const std::string& roomName);
  std::vector<std::string> getUserRooms(const std::string& userName);
  std::vector<std::string> getRooms();

  bool saveMessage(const std::string& roomName, const std::string& userName,
                   const std::string& message, int64_t timestamp);
  std::vector<nlohmann::json> getRoomMessages(const std::string& roomName,
                                              int64_t since = 0);

 private:
  bool initializeDatabase();
  bool executeQuery(const std::string& query);

  std::string dbPath_;
  sqlite3* db_;
  std::recursive_mutex dbMutex_;
};
