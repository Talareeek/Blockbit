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

    sf::Vector2<double> cursor_position;

    bool mid_attack = false;
    bool mid_usage = false;

    float since_last_use = 0.0f;

    bool drops = false;
    bool drops_full_stack = false;

    sf::Vector2i mining_block;
    float mining_time = 0.0f;
    

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

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<PlayerControlledComponent>(*this);
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
