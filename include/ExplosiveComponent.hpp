#ifndef EXPLOSIVE_COMPONENT_HPP
#define EXPLOSIVE_COMPONENT_HPP

#include <string>
#include <sstream>

struct ExplosiveComponent
{
    float force;

    bool destroyBlocks = true;
    bool damageEntities = true;

    float fuseTime = 3.0f;
    float timer = 0.0f;

    std::string serialize()
    {
        std::string output;

        output += std::to_string(force) + ' ';
        output += std::to_string(destroyBlocks ? 1 : 0) + ' ';
        output += std::to_string(damageEntities ? 1 : 0) + ' ';
        output += std::to_string(fuseTime) + ' ';
        output += std::to_string(timer) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        int destroyBlocksInt, damageEntitiesInt;

        iss >> force >> destroyBlocksInt >> damageEntitiesInt >> fuseTime >> timer;

        destroyBlocks = (destroyBlocksInt != 0);
        damageEntities = (damageEntitiesInt != 0);
    }

};

#endif // EXPLOSIVE_COMPONENT_HPP