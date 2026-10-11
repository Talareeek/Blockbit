#ifndef PREFAB_HPP
#define PREFAB_HPP

#include "Entity.hpp"

enum class PrefabID
{
    Player,
    Pig,
    Dynamite
};

class PrefabRegistry
{
private:

    static std::unordered_map<PrefabID, Entity> prefabs;

public:

    static void add(PrefabID prefab_id, Entity& entity);
    static const Entity& get(PrefabID prefab_id);

    static void init();
};

#endif // PREFAB_HPP