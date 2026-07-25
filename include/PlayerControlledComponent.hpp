#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <string>
#include <sstream>

struct PlayerControlledComponent : public Component
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

    std::string name() const override
    {
        return "PlayerControlledComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["client_id"] = Tag(clientId);
        compound["nickname"] = Tag(nickname);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        clientId = tag["client_id"].get<uint32_t>();
        nickname = tag["nickname"].get<std::string>();
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
