#include "user.hpp"

namespace chat {

nlohmann::json User::toJson() const {
    return {
        {"username", username},
        {"password", password},
        {"is_online", is_online}
    };
}

User User::fromJson(const nlohmann::json& j) {
    return User{
        j["username"].get<std::string>(),
        j["password"].get<std::string>(),
        j["is_online"].get<bool>()
    };
}

} // namespace chat
