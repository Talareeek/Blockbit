#include "../include/MobSpawnConfig.hpp"

namespace
{
    SpawnEntry pigEntry()
    {
        SpawnEntry e;
        e.prefab   = PrefabID::Pig;
        e.category = MobCategory::Passive;
        e.weight   = 10;
        e.min_pack = 2;
        e.max_pack = 4;
        e.surface.allowed_ground     = {BlockID::Grass};
        e.surface.required_air_above = 2;
        e.surface.requires_sky       = true;
        e.day_allowed                = true;
        e.night_allowed              = false;
        e.entity_size                = {1.0, 0.75};
        return e;
    }

    MobSpawnConfig makeDefault()
    {
        MobSpawnConfig cfg;

        cfg.categories[MobCategory::Hostile] =
            {.cap = 70, .spawn_interval = 0.1f, .min_radius = 24.0f, .max_radius = 128.0f,
             .despawn_radius = 128.0f, .attempts_per_cycle = 3};

        cfg.categories[MobCategory::Passive] =
            {.cap = 15, .spawn_interval = 20.0f, .min_radius = 24.0f, .max_radius = 96.0f,
             .despawn_radius = 160.0f, .attempts_per_cycle = 2};

        cfg.categories[MobCategory::Ambient] =
            {.cap = 15, .spawn_interval = 1.0f, .min_radius = 24.0f, .max_radius = 80.0f,
             .despawn_radius = 128.0f, .attempts_per_cycle = 1};

        cfg.categories[MobCategory::Water] =
            {.cap = 5, .spawn_interval = 1.0f, .min_radius = 24.0f, .max_radius = 128.0f,
             .despawn_radius = 128.0f, .attempts_per_cycle = 1};

        // Only Pig exists today; drop it in a few plausible overworld biomes.
        // Hostile/Ambient/Water tables stay empty — new entries added here will
        // start spawning without touching the system code.
        const SpawnEntry pig = pigEntry();

        for (Biome b : {Biome::Plains, Biome::Forest, Biome::Savanna,
                        Biome::Taiga, Biome::Jungle})
        {
            cfg.biome_tables[b].entries[MobCategory::Passive].push_back(pig);
        }

        return cfg;
    }
}

const MobSpawnConfig& defaultMobSpawnConfig()
{
    static const MobSpawnConfig instance = makeDefault();
    return instance;
}
