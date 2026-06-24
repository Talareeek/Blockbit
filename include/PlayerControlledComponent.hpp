#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include <cstdint>
#include <string>
#include <sstream>

struct PlayerControlledComponent
{
    uint32_t clientId = 0;

    PlayerControlledComponent() = default;
    PlayerControlledComponent(uint32_t clientId) : clientId(clientId) {}

    std::string serialize()
    {
        return std::to_string(clientId) + '\n';
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> clientId;
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
