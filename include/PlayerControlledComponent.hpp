#ifndef PLAYER_CONTROLLED_COMPONENT_HPP
#define PLAYER_CONTROLLED_COMPONENT_HPP

#include "Component.hpp"
#include "Input.hpp"
#include "View.hpp"
#include "PlayerIdentity.hpp"

#include <cstdint>
#include <string>
#include <sstream>
#include <deque>
#include <memory>

struct PlayerControlledComponent : public Component
{
    PlayerIdentity identity;
    bool active = true;

    sf::Vector2<double> cursor_position;

    bool mid_attack = false;
    bool mid_usage = false;

    float since_last_use = 0.0f;
    float since_last_attack = 0.0f;

    bool drops = false;
    bool drops_full_stack = false;

    sf::Vector2i mining_block;
    float mining_time = 0.0f;

    std::optional<ChangeSlotData> change_slot;

    bool jump = false;

    // Stateful movement intent. Set from the last MOVE input in each received
    // batch (and cleared when a batch arrives with no MOVE). Applied as force
    // every tick regardless of whether a new input batch came in, so network
    // jitter / single-packet loss no longer produces "impulse" movement on
    // the server. Not serialized — the client tracks its own copy for
    // prediction and the server's authoritative state is Transform/Physics,
    // not the intent itself.
    sf::Vector2<float> move_intent{0.0f, 0.0f};
    bool move_sprint = false;

    bool noclip = false;

    // Transient per-tick request queue: anything the player interacts with
    // (an item's onUse, a block, etc.) can push a ready-to-run ServerView
    // onto this queue and GameServer will hand it to the owning client as
    // the active content view. Not serialized.
    std::deque<std::unique_ptr<ServerView>> pending_view_opens;

    PlayerControlledComponent() = default;
    PlayerControlledComponent(PlayerIdentity identity) : identity{std::move(identity)}
    {

    }

    std::string serialize()
    {
        return identity.display_name + '\n';
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        iss >> identity.display_name;
        if (identity.id == 0) identity.id = hashDisplayName(identity.display_name);
    }

    std::string name() const override
    {
        return "PlayerControlledComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        // "nickname" kept for backward compat with pre-identity saves; readers
        // that still key on it see the display name.
        compound["nickname"] = Tag(identity.display_name);
        compound["identity_id"] = Tag(identity.id);
        compound["mid_attack"] = Tag(mid_attack);
        compound["mining_block"] = Tag(mining_block);
        compound["mining_time"] = Tag(mining_time);
        compound["noclip"] = Tag(noclip);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        const TagCompound& compound = tag.get<TagCompound>();

        identity.display_name = tag["nickname"].get<std::string>();
        // Pre-identity saves lack identity_id; fall back to a deterministic
        // hash so the migrated player maps to the same ban/op records.
        if (compound.count("identity_id"))
            identity.id = tag["identity_id"].get<uint64_t>();
        else
            identity.id = hashDisplayName(identity.display_name);

        if (compound.count("mid_attack"))   mid_attack   = tag["mid_attack"].get<bool>();
        if (compound.count("mining_block")) mining_block = tag["mining_block"].get<sf::Vector2i>();
        if (compound.count("mining_time"))  mining_time  = tag["mining_time"].get<float>();
        if (compound.count("noclip"))       noclip       = tag["noclip"].get<bool>();
    }

    std::unique_ptr<Component> clone() const override
    {
        // pending_view_opens is transient per-tick state and holds move-only
        // unique_ptrs, so we build the clone field-by-field instead of copying
        // the whole struct.
        auto out = std::make_unique<PlayerControlledComponent>(identity);
        out->active            = active;
        out->cursor_position   = cursor_position;
        out->mid_attack        = mid_attack;
        out->mid_usage         = mid_usage;
        out->since_last_use    = since_last_use;
        out->since_last_attack = since_last_attack;
        out->drops             = drops;
        out->drops_full_stack  = drops_full_stack;
        out->mining_block      = mining_block;
        out->mining_time       = mining_time;
        out->change_slot       = change_slot;
        out->jump              = jump;
        out->move_intent       = move_intent;
        out->move_sprint       = move_sprint;
        out->noclip            = noclip;
        return out;
    }
};

#endif // PLAYER_CONTROLLED_COMPONENT_HPP
