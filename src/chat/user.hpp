#pragma once
#include <nlohmann/json.hpp>
#include <string>

/**
 * @brief 表示聊天系统中的一个用户
 *
 * 提供JSON序列化和反序列化
 */

struct User {
  std::string username;
  std::string password;
  bool is_online;

  nlohmann::json toJson() const;
  static User fromJson(const nlohmann::json& j);
};
