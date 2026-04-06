#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

#include <cstdint>
#include <string>
#include <sstream>

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

    std::string serialize()
    {
        std::string output;

        output += std::to_string(health) + ' ';
        output += std::to_string(maxHealth) + ' ';
        output += std::to_string(killOnZero ? 1 : 0) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        int killOnZeroInt;

        iss >> health >> maxHealth >> killOnZeroInt;

        killOnZero = (killOnZeroInt != 0);
    }

};

#endif // HEALTH_COMPONENT_HPP