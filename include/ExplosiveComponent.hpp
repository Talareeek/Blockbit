#ifndef EXPLOSIVE_COMPONENT_HPP
#define EXPLOSIVE_COMPONENT_HPP

#include "Component.hpp"

#include <string>
#include <sstream>

struct ExplosiveComponent : public Component
{
    float force;

    bool destroyBlocks = true;
    bool damageEntities = true;

    float fuseTime = 3.0f;
    float timer = 0.0f;

    ExplosiveComponent(
    float force = 0.0f,
    bool destroyBlocks = true,
    bool damageEntities = true,
    float fuseTime = 3.0f,
    float timer = 0.0f
    )
        : force(force),
        destroyBlocks(destroyBlocks),
        damageEntities(damageEntities),
        fuseTime(fuseTime),
        timer(timer)
    {
    }

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

    std::string name() const override
    {
        return "ExplosiveComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["force"] = Tag(force);
        compound["destroy_blocks"] = Tag(destroyBlocks);
        compound["damage_entities"] = Tag(damageEntities);
        compound["fuse_time"] = Tag(fuseTime);
        compound["timer"] = Tag(timer);

        return Tag(compound);
    }


    void deserialize(const Tag& tag) override
    {
        force = tag["force"].get<float>();
        destroyBlocks = tag["destroy_blocks"].get<bool>();
        damageEntities = tag["damage_entities"].get<bool>();
        fuseTime = tag["fuse_time"].get<float>();
        timer = tag["timer"].get<float>();
    }
};

#endif // EXPLOSIVE_COMPONENT_HPP