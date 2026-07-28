#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <string>
#include <sstream>

struct PlayerControlledComponent : public Component
{
    std::string nickname;
    bool active = true;

    PlayerControlledComponent() = default;
    PlayerControlledComponent(std::string nickname) : nickname{nickname}
    {

    }

    std::string serialize()
    {
        return nickname + '\n';
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> nickname;
    }

    std::string name() const override
    {
        return "PlayerControlledComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["nickname"] = Tag(nickname);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        nickname = tag["nickname"].get<std::string>();
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
