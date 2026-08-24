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

    bool killOnZero = true;

    HealthComponent()
    {
        
    }

    HealthComponent(uint32_t health, uint32_t maxHealth, bool killOnZero = true) : health(health), maxHealth(maxHealth), killOnZero(killOnZero)
    {

    }

    std::string serialize()
    {
        std::string output;

        output += std::to_string(health) + ' ';
        output += std::to_string(maxHealth) + ' ';
        output += std::to_string(killOnZero ? 1 : 0) + '\n';

        return output;
    }

    std::string name() const override
    {
        return "HealthComponent";
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        int killOnZeroInt;

        iss >> health >> maxHealth >> killOnZeroInt;

        killOnZero = (killOnZeroInt != 0);
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["health"] = Tag(health);
        compound["max_health"] = Tag(maxHealth);
        compound["kill_on_zero"] = Tag(killOnZero);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        health = tag["health"].get<uint32_t>();
        maxHealth = tag["max_health"].get<uint32_t>();
        killOnZero = tag["kill_on_zero"].get<bool>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<HealthComponent>(*this);
    }

};

#endif // HEALTH_COMPONENT_HPP