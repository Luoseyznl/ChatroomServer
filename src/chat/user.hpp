#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace chat {

struct User {
    std::string username;
    std::string password;
    bool is_online;

    nlohmann::json toJson() const;
    static User fromJson(const nlohmann::json& j);
};

} // namespace chat
