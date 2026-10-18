#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <string>
#include <sstream>

struct HealthComponent : public Component
{
    static constexpr float DAMAGE_FLASH_DURATION = 0.5f;

    uint32_t health = 0;
    uint32_t maxHealth = 0;
    bool damageable = false;

    float since_last_damaged = DAMAGE_FLASH_DURATION;

    HealthComponent()
    {

    }

    HealthComponent(uint32_t health, uint32_t maxHealth, bool damageable = false) : health(health), maxHealth(maxHealth), damageable(damageable)
    {

    }

    std::string name() const override
    {
        return "HealthComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["health"] = Tag(health);
        compound["max_health"] = Tag(maxHealth);
        compound["damageable"] = Tag(damageable);
        compound["since_last_damaged"] = Tag(since_last_damaged);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        health = tag["health"].get<uint32_t>();
        maxHealth = tag["max_health"].get<uint32_t>();

        const auto& compound = tag.get<TagCompound>();

        if(compound.count("damageable")) damageable = tag["damageable"].get<bool>();
        if(compound.count("since_last_damaged")) since_last_damaged = tag["since_last_damaged"].get<float>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<HealthComponent>(*this);
    }

};

#endif // HEALTH_COMPONENT_HPP