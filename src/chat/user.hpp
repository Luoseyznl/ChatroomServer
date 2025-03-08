#pragma once
#include <string>
#include <nlohmann/json.hpp>


struct User {
    std::string username;
    std::string password;
    bool is_online;

    nlohmann::json toJson() const;
    static User fromJson(const nlohmann::json& j);
};


