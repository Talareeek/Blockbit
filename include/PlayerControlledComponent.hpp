#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include "Component.hpp"
#include "Input.hpp"
#include "View.hpp"

#include <cstdint>
#include <string>
#include <sstream>
#include <deque>
#include <memory>

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

    std::optional<ChangeSlotData> change_slot;

    bool jump = false;

    sf::Vector2<double> move_direction;

    // Transient per-tick request queue: anything the player interacts with
    // (an item's onUse, a block, etc.) can push a ready-to-run ServerView
    // onto this queue and GameServer will hand it to the owning client as
    // the active content view. Not serialized.
    std::deque<std::unique_ptr<ServerView>> pending_view_opens;

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
        compound["mid_attack"] = Tag(mid_attack);
        compound["mining_block"] = Tag(mining_block);
        compound["mining_time"] = Tag(mining_time);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        nickname = tag["nickname"].get<std::string>();

        const TagCompound& compound = tag.get<TagCompound>();
        if (compound.count("mid_attack"))   mid_attack   = tag["mid_attack"].get<bool>();
        if (compound.count("mining_block")) mining_block = tag["mining_block"].get<sf::Vector2i>();
        if (compound.count("mining_time"))  mining_time  = tag["mining_time"].get<float>();
    }

    std::unique_ptr<Component> clone() const override
    {
        // pending_view_opens is transient per-tick state and holds move-only
        // unique_ptrs, so we build the clone field-by-field instead of copying
        // the whole struct.
        auto out = std::make_unique<PlayerControlledComponent>(nickname);
        out->active            = active;
        out->cursor_position   = cursor_position;
        out->mid_attack        = mid_attack;
        out->mid_usage         = mid_usage;
        out->since_last_use    = since_last_use;
        out->drops             = drops;
        out->drops_full_stack  = drops_full_stack;
        out->mining_block      = mining_block;
        out->mining_time       = mining_time;
        out->change_slot       = change_slot;
        out->jump              = jump;
        out->move_direction    = move_direction;
        return out;
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
