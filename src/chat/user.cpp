#include "user.hpp"

nlohmann::json User::toJson() const {
  return nlohmann::json{
      {"userName", userName}, {"password", password}, {"isOnline", isOnline}};
}

User User::fromJson(const nlohmann::json& j) {
  User user;
  user.userName = j.at("userName").get<std::string>();
  user.password = j.at("password").get<std::string>();
  user.isOnline = j.at("isOnline").get<bool>();
  return user;
}