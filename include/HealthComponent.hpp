#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

#include <cstdint>

struct HealthComponent
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
};

#endif // HEALTH_COMPONENT_HPP