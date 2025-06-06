#include "database_manager.hpp"

#include <sstream>

#include "../utils/logger.hpp"

DatabaseManager::DatabaseManager(const std::string& db_path)
    : db_path_(db_path), db_(nullptr) {
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc) {
      LOG_ERROR << "Can't open database: " << sqlite3_errmsg(db_);
      return;
    }
    LOG_INFO << "Opened database successfully";
  }
  // 初始化数据库表
  if (!initializeTables()) {
    LOG_ERROR << "Failed to initialize database tables";
    sqlite3_close(db_);
    db_ = nullptr;
    return;
  }
}

DatabaseManager::~DatabaseManager() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (db_) {
    sqlite3_close(db_);
  }
}

/**
 * @brief 初始化数据库表
 *
 * 1. users - 存储用户信息
 * 2. rooms - 存储聊天室信息
 * 3. room_members - 存储聊天室成员关系
 * 4. messages - 存储聊天消息
 *
 * @return bool 操作是否成功
 */
bool DatabaseManager::initializeTables() {
  // 1 用户表 users
  const char* create_users_table =
      "CREATE TABLE IF NOT EXISTS users ("
      "username TEXT PRIMARY KEY,"             // 用户名（主键）
      "password_hash TEXT NOT NULL,"           // 密码哈希
      "created_at INTEGER NOT NULL,"           // 创建时间戳
      "is_online INTEGER DEFAULT 0,"           // 在线状态（0: 离线, 1: 在线）
      "last_active_time INTEGER DEFAULT 0);";  // 最后活跃时间戳

  // 2 房间表 rooms
  const char* create_rooms_table =
      "CREATE TABLE IF NOT EXISTS rooms ("
      "name TEXT PRIMARY KEY,"                              // 房间名称（主键）
      "creator TEXT NOT NULL,"                              // 创建者用户名
      "created_at INTEGER NOT NULL,"                        // 创建时间戳
      "FOREIGN KEY(creator) REFERENCES users(username));";  // 外键引用

  // 3 房间成员表 room_members
  const char* create_room_members_table =
      "CREATE TABLE IF NOT EXISTS room_members ("
      "room_name TEXT NOT NULL,"           // 房间名称
      "username TEXT NOT NULL,"            // 用户名
      "joined_at INTEGER NOT NULL,"        // 加入时间戳
      "PRIMARY KEY(room_name, username),"  // 复合主键（房间名, 用户名）
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"       // 外键引用
      "FOREIGN KEY(username) REFERENCES users(username));";  // 外键引用

  // 4 消息表 messages
  const char* create_messages_table =
      "CREATE TABLE IF NOT EXISTS messages ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"                // 消息ID（主键）
      "room_name TEXT NOT NULL,"                             // 房间名称
      "username TEXT NOT NULL,"                              // 发送者用户名
      "content TEXT NOT NULL,"                               // 消息内容
      "timestamp INTEGER NOT NULL,"                          // 时间戳
      "FOREIGN KEY(room_name) REFERENCES rooms(name),"       // 外键引用
      "FOREIGN KEY(username) REFERENCES users(username));";  // 外键引用

  return executeQuery(create_users_table) && executeQuery(create_rooms_table) &&
         executeQuery(create_room_members_table) &&
         executeQuery(create_messages_table);
}

/**
 * @brief 执行SQL查询语句
 *
 * @param query SQL查询字符串
 * @return bool 操作是否成功
 */
bool DatabaseManager::executeQuery(const std::string& query) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  char* err_msg = nullptr;

  /*
   * sqlite3_exec 用法概览：
   * int sqlite3_exec(sqlite3 *db, const char *sql,
   *                   int (*callback)(void*,int,char**,char**),
   *                   void *arg, char **errmsg);
   * - db: 已打开的数据库句柄
   * - sql: 要执行的 SQL 语句（可以是多条，分号分隔,\0结尾）
   * - callback: 查询结果集回调函数，接收每一行结果，可为 NULL
   * - arg: 传给回调函数的用户参数，可以使用 std::tuple 或 std::vector
   * - errmsg: 返回的错误信息字符串，需使用 sqlite3_free() 释放
   * 返回值：SQLITE_OK 表示成功
   */
  int rc = sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg);

  if (rc != SQLITE_OK) {
    LOG_ERROR << "SQL error: " << err_msg;
    sqlite3_free(err_msg);  // 释放错误信息字符串
    return false;
  }
  return true;
}

/**
 * @brief 创建新用户
 *
 * @param username 用户名
 * @param password_hash 密码哈希
 * @return bool 操作是否成功
 */
bool DatabaseManager::createUser(const std::string& username,
                                 const std::string& password_hash) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "INSERT INTO users (username, password_hash, created_at) VALUES ('"
     << username << "', '" << password_hash << "', "
     << std::chrono::system_clock::now().time_since_epoch().count() << ");";
  return executeQuery(ss.str());
}

/**
 * @brief 验证用户凭据
 *
 * @param username 用户名
 * @param password_hash 密码哈希
 * @return bool 凭据是否有效
 */
bool DatabaseManager::validateUser(const std::string& username,
                                   const std::string& password_hash) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM users WHERE username = '" << username
     << "' AND password_hash = '" << password_hash << "';";

  sqlite3_stmt* stmt;  // SQLite 预编译语句
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return false;
  }

  bool valid = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {  // 执行查询并获取结果
    valid = (sqlite3_column_int(stmt, 0) > 0);
  }

  sqlite3_finalize(stmt);  // 释放预处理语句
  return valid;
}

/**
 * @brief 检查用户是否存在
 *
 * @param username 用户名
 * @return bool 用户是否存在
 */
bool DatabaseManager::userExists(const std::string& username) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "SELECT COUNT(*) FROM users WHERE username = '" << username << "';";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return false;
  }

  bool exists = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    exists = (sqlite3_column_int(stmt, 0) > 0);
  }

  sqlite3_finalize(stmt);
  return exists;
}

/**
 * @brief 创建新聊天室
 *
 * @param name 聊天室名称
 * @param creator 创建者用户名
 * @return bool 操作是否成功
 */
bool DatabaseManager::createRoom(const std::string& name,
                                 const std::string& creator) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "INSERT INTO rooms (name, creator, created_at) VALUES ('" << name
     << "', '" << creator << "', "
     << std::chrono::system_clock::now().time_since_epoch().count() << ");";
  return executeQuery(ss.str());
}

/**
 * @brief 删除聊天室
 *
 * @param name 聊天室名称
 * @return bool 操作是否成功
 */
bool DatabaseManager::deleteRoom(const std::string& name) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "DELETE FROM rooms WHERE name = '" << name << "';";

  // 执行SQL语句（外键约束会自动删除相关的成员和消息记录）
  return executeQuery(ss.str());
}

/**
 * @brief 获取所有聊天室
 *
 * @return std::vector<std::string> 聊天室名称列表
 */
std::vector<std::string> DatabaseManager::getRooms() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<std::string> rooms;
  const char* query = "SELECT name FROM rooms;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
    return rooms;
  }

  // 让 SQLite 内部虚拟机（VM）步进查询，取出每一结果行的第0列的文本值（房间名）
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    rooms.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return rooms;
}

bool DatabaseManager::addRoomMember(const std::string& room_name,
                                    const std::string& username) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  // 首先检查用户是否已经在房间中
  std::stringstream check_ss;
  check_ss << "SELECT COUNT(*) FROM room_members WHERE room_name = '"
           << room_name << "' AND username = '" << username << "';";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, check_ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
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
  insert_ss
      << "INSERT INTO room_members (room_name, username, joined_at) VALUES ('"
      << room_name << "', '" << username << "', "
      << std::chrono::system_clock::now().time_since_epoch().count() << ");";
  return executeQuery(insert_ss.str());
}

bool DatabaseManager::removeRoomMember(const std::string& room_name,
                                       const std::string& username) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "DELETE FROM room_members WHERE room_name = '" << room_name
     << "' AND username = '" << username << "';";
  return executeQuery(ss.str());
}

std::vector<std::string> DatabaseManager::getRoomMembers(
    const std::string& room_name) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<std::string> members;
  std::stringstream ss;
  ss << "SELECT username FROM room_members WHERE room_name = '" << room_name
     << "';";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return members;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    members.push_back(
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
  }

  sqlite3_finalize(stmt);
  return members;
}

bool DatabaseManager::saveMessage(const std::string& room_name,
                                  const std::string& username,
                                  const std::string& content,
                                  int64_t timestamp) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "INSERT INTO messages (room_name, username, content, timestamp) VALUES "
        "('"
     << room_name << "', '" << username << "', '" << content << "', "
     << timestamp << ");";
  return executeQuery(ss.str());
}

std::vector<nlohmann::json> DatabaseManager::getMessages(
    const std::string& room_name, int64_t since) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<nlohmann::json> messages;
  std::stringstream ss;
  ss << "SELECT username, content, timestamp FROM messages WHERE room_name = '"
     << room_name << "' ORDER BY timestamp ASC;";

  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, ss.str().c_str(), -1, &stmt, nullptr) !=
      SQLITE_OK) {
    return messages;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    nlohmann::json msg;
    msg["username"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    msg["content"] =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    msg["timestamp"] = sqlite3_column_int64(stmt, 2);
    messages.push_back(msg);
  }

  sqlite3_finalize(stmt);
  return messages;
}

bool DatabaseManager::setUserOnlineStatus(const std::string& username,
                                          bool is_online) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "UPDATE users SET is_online = " << (is_online ? 1 : 0);
  if (is_online) {
    ss << ", last_active_time = "
       << std::chrono::system_clock::now().time_since_epoch().count();
  }
  ss << " WHERE username = '" << username << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::updateUserLastActiveTime(const std::string& username) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::stringstream ss;
  ss << "UPDATE users SET last_active_time = "
     << std::chrono::system_clock::now().time_since_epoch().count()
     << " WHERE username = '" << username << "';";
  return executeQuery(ss.str());
}

bool DatabaseManager::checkAndUpdateInactiveUsers(int64_t timeout_ms) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  int64_t current_time =
      std::chrono::system_clock::now().time_since_epoch().count();
  int64_t timeout_time = current_time - timeout_ms;

  std::stringstream ss;
  ss << "UPDATE users SET is_online = 0 WHERE is_online = 1 AND "
     << "last_active_time < " << timeout_time << ";";
  return executeQuery(ss.str());
}

std::vector<User> DatabaseManager::getAllUsers() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<User> users;
  const char* query = "SELECT username, password_hash, is_online FROM users;";
  sqlite3_stmt* stmt;

  if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
    LOG_ERROR << "Failed to prepare statement: " << sqlite3_errmsg(db_);
    return users;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char* username =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    const char* password =
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    bool is_online = sqlite3_column_int(stmt, 2) > 0;
    users.push_back({std::string(username), std::string(password), is_online});
  }

  sqlite3_finalize(stmt);
  return users;
}
