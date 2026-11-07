#ifndef EFFECTS_COMPONENT_HPP
#define EFFECTS_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <string>
#include <vector>

enum class EffectID : uint32_t
{
    None         = 0,
    Regeneration = 1,
    Poison       = 2,
    Burning      = 3,
    Starving     = 4
};

struct EffectInstance
{
    EffectID id = EffectID::None;

    float interval = 1.0f;

    // Seconds since the registry tick last ran for this instance. When it
    // crosses `interval`, the registry fires and this resets to 0.
    float since_last_tick = 0.0f;

    // < 0.0f means the effect never expires on its own.
    float remaining_duration = -1.0f;

    uint32_t magnitude = 1;
};

struct EffectsComponent : public Component
{
    std::vector<EffectInstance> effects;

    EffectsComponent() = default;

    std::string name() const override
    {
        return "EffectsComponent";
    }

    ComponentSide side() const override
    {
        return ComponentSide::Server;
    }

    Tag serialize() const override
    {
        TagCompound compound;

        TagList list;
        list.reserve(effects.size());
        for (const auto& e : effects)
        {
            TagCompound entry;
            entry["id"]                 = Tag(static_cast<uint32_t>(e.id));
            entry["interval"]           = Tag(e.interval);
            entry["since_last_tick"]    = Tag(e.since_last_tick);
            entry["remaining_duration"] = Tag(e.remaining_duration);
            entry["magnitude"]          = Tag(e.magnitude);
            list.push_back(Tag(entry));
        }
        compound["effects"] = Tag(list);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        effects.clear();

        const auto& compound = tag.get<TagCompound>();
        if (!compound.count("effects")) return;

        const auto& list = tag["effects"].get<TagList>();
        effects.reserve(list.size());

        for (const auto& item : list)
        {
            EffectInstance e;
            e.id                 = static_cast<EffectID>(item["id"].get<uint32_t>());
            e.interval           = item["interval"].get<float>();
            e.since_last_tick    = item["since_last_tick"].get<float>();
            e.remaining_duration = item["remaining_duration"].get<float>();
            e.magnitude          = item["magnitude"].get<uint32_t>();
            effects.push_back(e);
        }
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<EffectsComponent>(*this);
    }
};

#endif // EFFECTS_COMPONENT_HPP
