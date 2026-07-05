#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include <cstdint>
#include <string>
#include <sstream>

struct PlayerControlledComponent
{
    uint32_t clientId = 0;
    std::string nickname;
    bool active = false;

    PlayerControlledComponent() = default;
    PlayerControlledComponent(uint32_t clientId) : clientId(clientId) {}

    std::string serialize()
    {
        return std::to_string(clientId) + '\n' + nickname + '\n';
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> clientId;
        iss >> nickname;
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
