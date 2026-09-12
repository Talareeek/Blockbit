#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

#include "Component.hpp"

#include <cstdint>
#include <string>
#include <sstream>

struct HealthComponent : public Component
{
    uint32_t health = 0;
    uint32_t maxHealth = 0;

    HealthComponent()
    {
        
    }

    HealthComponent(uint32_t health, uint32_t maxHealth) : health(health), maxHealth(maxHealth)
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

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        health = tag["health"].get<uint32_t>();
        maxHealth = tag["max_health"].get<uint32_t>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<HealthComponent>(*this);
    }

};

#endif // HEALTH_COMPONENT_HPP