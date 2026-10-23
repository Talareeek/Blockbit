#ifndef MOB_SPAWN_CONFIG_HPP
#define MOB_SPAWN_CONFIG_HPP

#include "Block.hpp"
#include "Climate.hpp"
#include "Prefab.hpp"

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

enum class MobCategory : uint8_t
{
    Hostile = 0,
    Passive = 1,
    Ambient = 2,
    Water   = 3
};

struct SurfaceRule
{
    // Ground block right below the spawn tile must be one of these.
    // Empty => any non-air solid is accepted.
    std::vector<BlockID> allowed_ground;

    // Air column required above the spawn tile.
    int required_air_above = 2;

    // Sky-exposure requirement for the spawn tile.
    bool requires_sky = false;
    bool forbids_sky  = false;

    // Spawn tile must be inside water (Water surface mobs).
    bool requires_water = false;
};

struct SpawnEntry
{
    PrefabID prefab;
    MobCategory category;

    int weight   = 1;
    int min_pack = 1;
    int max_pack = 1;

    SurfaceRule surface;

    // Time-of-day gate. The game has no block-level lighting, so "light"
    // reduces to day/night (via World::dayTime). "Darkness" of caves is
    // expressed via SurfaceRule::forbids_sky, not here.
    bool day_allowed   = true;
    bool night_allowed = true;

    // AABB used for collision validation at the proposed position.
    sf::Vector2<double> entity_size{1.0, 1.0};
};

struct CategoryRule
{
    // Hard population cap for this category across the whole world.
    // 0 disables spawning for the category.
    int cap = 0;

    // Seconds between spawn attempts for the category.
    float spawn_interval = 1.0f;

    // Ring around each player where spawn anchors are sampled.
    float min_radius = 24.0f;
    float max_radius = 128.0f;

    // Entities of this category further than this from every player are
    // despawned. Set to 0 to disable despawn for the category.
    float despawn_radius = 128.0f;

    // How many anchor rolls we try per interval, per player.
    int attempts_per_cycle = 3;
};

struct BiomeSpawnTable
{
    std::unordered_map<MobCategory, std::vector<SpawnEntry>> entries;
};

struct MobSpawnConfig
{
    std::unordered_map<MobCategory, CategoryRule> categories;
    std::unordered_map<Biome, BiomeSpawnTable>    biome_tables;
};

const MobSpawnConfig& defaultMobSpawnConfig();

#endif // MOB_SPAWN_CONFIG_HPP
