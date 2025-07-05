#pragma once

#include <nlohmann/json.hpp>
#include <string>

struct User {
  std::string userName;
  std::string password;
  bool isOnline;

  nlohmann::json toJson() const;
  static User fromJson(const nlohmann::json& j);
};