#include "../include/MobSpawnSystem.hpp"

#include "../include/ServerWorld.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/AIComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/Block.hpp"
#include "../include/Chunk.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
#include <vector>

namespace
{
    std::mt19937& rng()
    {
        static thread_local std::mt19937 generator{std::random_device{}()};
        return generator;
    }

    float uniform(float lo, float hi)
    {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng());
    }

    int uniformInt(int lo, int hi)
    {
        if (lo >= hi) return lo;
        std::uniform_int_distribution<int> d(lo, hi);
        return d(rng());
    }

    // Sum of two U(-1,1) → triangular distribution on [-2, 2] peaking at 0.
    float triangularOffset()
    {
        return uniform(-1.0f, 1.0f) + uniform(-1.0f, 1.0f);
    }

    int worldXToChunk(int wx)
    {
        return (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    }

    bool isOpaqueForSky(const Block& b)
    {
        if (b.id == BlockID::Air) return false;
        auto it = blockDatabase.find(b.id);
        if (it == blockDatabase.end()) return true;
        return !it->second.transparent;
    }

    bool isSkyExposed(World& world, int wx, int wy)
    {
        for (int y = wy + 1; y < CHUNK_HEIGHT; ++y)
        {
            if (isOpaqueForSky(world.getBlock(wx, y))) return false;
        }
        return true;
    }

    bool isDaytime(World& world)
    {
        return world.getDayTime() < World::DAY_CYCLE_DURATION * 0.5f;
    }

    // Classify an existing mob entity into a category so caps and despawn
    // ranges stay in sync with what the system spawns. Ambient/Water need
    // their own component signal once those mobs exist; for now only AI
    // personality is available.
    MobCategory classifyMob(const Entity& entity)
    {
        const auto& ai = entity.getComponent<AIComponent>();
        switch (ai.personality)
        {
            case AIComponent::Personality::Aggressive: return MobCategory::Hostile;
            case AIComponent::Personality::Passive:    return MobCategory::Passive;
        }
        return MobCategory::Passive;
    }

    bool isMob(const Entity& entity)
    {
        return entity.hasComponent<AIComponent>()
            && entity.hasComponent<TransformComponent>()
            && !entity.hasComponent<PlayerControlledComponent>();
    }

    std::unordered_map<MobCategory, int> countMobs(World& world)
    {
        std::unordered_map<MobCategory, int> counts;
        for (const auto& [id, entity] : world.getEntities())
        {
            if (!isMob(entity)) continue;
            ++counts[classifyMob(entity)];
        }
        return counts;
    }

    std::vector<UUID> collectPlayers(World& world)
    {
        std::vector<UUID> out;
        for (const auto& [id, entity] : world.getEntities())
        {
            if (entity.hasComponent<PlayerControlledComponent>()
                && entity.hasComponent<TransformComponent>())
            {
                out.push_back(id);
            }
        }
        return out;
    }

    double distanceToNearestPlayer(World& world, sf::Vector2<double> pos,
                                   const std::vector<UUID>& players)
    {
        double best = std::numeric_limits<double>::infinity();
        for (UUID id : players)
        {
            if (!world.doesEntityExist(id)) continue;
            const auto& t = world.getEntity(id).getComponent<TransformComponent>();
            sf::Vector2<double> c = t.center();
            double dx = c.x - pos.x;
            double dy = c.y - pos.y;
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < best) best = d;
        }
        return best;
    }

    // Find the highest-y column position that could serve as a surface spawn
    // tile (first air block sitting on top of a non-air block). Returns -1
    // when the column is all air or chunk is ungenerated.
    int findSurfaceY(World& world, int wx)
    {
        int chunk_pos = worldXToChunk(wx);
        auto it = world.getChunks().find(chunk_pos);
        if (it == world.getChunks().end() || !it->second.generated) return -1;

        for (int y = CHUNK_HEIGHT - 2; y >= 1; --y)
        {
            if (world.getBlock(wx, y).id == BlockID::Air
                && world.getBlock(wx, y - 1).id != BlockID::Air)
            {
                return y;
            }
        }
        return -1;
    }

    bool collidesWithEntity(World& world,
                            sf::Vector2<double> pos,
                            sf::Vector2<double> size)
    {
        sf::Rect<double> candidate(pos, size);
        for (const auto& [id, entity] : world.getEntities())
        {
            if (!entity.hasComponent<TransformComponent>()) continue;
            if (!entity.hasComponent<PhysicsComponent>()) continue;
            if (!entity.getComponent<PhysicsComponent>().isSolid) continue;

            const auto& t = entity.getComponent<TransformComponent>();
            sf::Rect<double> other(t.position, t.size);
            if (candidate.findIntersection(other).has_value()) return true;
        }
        return false;
    }

    bool groundAllowed(const SurfaceRule& rule, BlockID ground)
    {
        if (rule.allowed_ground.empty())
        {
            if (ground == BlockID::Air) return false;
            auto it = blockDatabase.find(ground);
            if (it == blockDatabase.end()) return false;
            return it->second.solid;
        }
        return std::find(rule.allowed_ground.begin(),
                         rule.allowed_ground.end(),
                         ground) != rule.allowed_ground.end();
    }

    bool canSpawnHere(World& world, const SpawnEntry& entry,
                      int wx, int wy, bool day)
    {
        if (day && !entry.day_allowed) return false;
        if (!day && !entry.night_allowed) return false;

        // Headroom.
        for (int i = 0; i < entry.surface.required_air_above; ++i)
        {
            if (world.getBlock(wx, wy + i).id != BlockID::Air) return false;
        }

        // Water-surface mobs want the spawn tile itself to be water.
        if (entry.surface.requires_water)
        {
            if (world.getBlock(wx, wy).id != BlockID::Water) return false;
        }
        else
        {
            BlockID ground = world.getBlock(wx, wy - 1).id;
            if (!groundAllowed(entry.surface, ground)) return false;
        }

        bool sky = isSkyExposed(world, wx, wy);
        if (entry.surface.requires_sky && !sky) return false;
        if (entry.surface.forbids_sky  &&  sky) return false;

        sf::Vector2<double> pos{
            static_cast<double>(wx) + 0.5 - entry.entity_size.x / 2.0,
            static_cast<double>(wy)
        };
        if (collidesWithEntity(world, pos, entry.entity_size)) return false;

        return true;
    }

    const SpawnEntry* pickWeighted(const std::vector<SpawnEntry>& entries)
    {
        int total = 0;
        for (const auto& e : entries) total += std::max(0, e.weight);
        if (total <= 0) return nullptr;

        int roll = uniformInt(1, total);
        for (const auto& e : entries)
        {
            roll -= std::max(0, e.weight);
            if (roll <= 0) return &e;
        }
        return &entries.back();
    }

    void despawnFarMobs(ServerWorld& world,
                        const MobSpawnConfig& config,
                        const std::vector<UUID>& players)
    {
        if (players.empty()) return;

        std::vector<UUID> doomed;
        for (const auto& [id, entity] : world.getEntities())
        {
            if (!isMob(entity)) continue;

            MobCategory cat = classifyMob(entity);
            auto rule_it = config.categories.find(cat);
            if (rule_it == config.categories.end()) continue;

            float radius = rule_it->second.despawn_radius;
            if (radius <= 0.0f) continue;

            sf::Vector2<double> center =
                entity.getComponent<TransformComponent>().center();

            double d = distanceToNearestPlayer(world, center, players);
            if (d > static_cast<double>(radius)) doomed.push_back(id);
        }

        for (UUID id : doomed) world.removeEntity(id);
    }

    void tryPackSpawn(ServerWorld& world,
                      const MobSpawnConfig& config,
                      MobCategory category,
                      const CategoryRule& rule,
                      std::unordered_map<MobCategory, int>& counts,
                      UUID player_id)
    {
        if (!world.doesEntityExist(player_id)) return;
        const auto& player_t =
            world.getEntity(player_id).getComponent<TransformComponent>();
        sf::Vector2<double> player_center = player_t.center();

        for (int attempt = 0; attempt < rule.attempts_per_cycle; ++attempt)
        {
            if (counts[category] >= rule.cap) return;

            // Horizontal ring: uniform in [min_radius, max_radius] on either side.
            float sign = (uniform(0.0f, 1.0f) < 0.5f) ? -1.0f : 1.0f;
            float dist = uniform(rule.min_radius, rule.max_radius);
            int anchor_x = static_cast<int>(
                std::floor(player_center.x + sign * dist));

            // Only consider loaded chunks: an unloaded column has no terrain to
            // walk and would spawn mobs the clients will never see.
            int anchor_chunk = worldXToChunk(anchor_x);
            if (!world.isChunkLoaded(anchor_chunk)) continue;

            Biome biome = world.biomeAt(anchor_x);
            auto biome_it = config.biome_tables.find(biome);
            if (biome_it == config.biome_tables.end()) continue;

            auto entries_it = biome_it->second.entries.find(category);
            if (entries_it == biome_it->second.entries.end()) continue;
            const auto& entries = entries_it->second;
            if (entries.empty()) continue;

            const SpawnEntry* picked = pickWeighted(entries);
            if (!picked) continue;

            int pack_size = uniformInt(picked->min_pack, picked->max_pack);
            const float PACK_SPREAD = 3.0f;

            for (int i = 0; i < pack_size; ++i)
            {
                if (counts[category] >= rule.cap) return;

                int wx = anchor_x + static_cast<int>(
                    std::round(triangularOffset() * PACK_SPREAD));

                int surface_y = findSurfaceY(world, wx);
                if (surface_y < 0) continue;

                if (!canSpawnHere(world, *picked, wx, surface_y, isDaytime(world))) continue;

                sf::Vector2<double> pos{
                    static_cast<double>(wx) + 0.5 - picked->entity_size.x / 2.0,
                    static_cast<double>(surface_y)
                };
                world.spawn(picked->prefab, pos);
                ++counts[category];
            }

            return; // One pack attempt per player, per cycle.
        }
    }
}

void MobSpawnSystem(ServerWorld& world, float dt, const MobSpawnConfig& config)
{
    // Per-category accumulators. Static state is fine: the server owns a
    // single world and the system is driven from one tick loop.
    static std::unordered_map<MobCategory, float> timers;

    std::vector<UUID> players = collectPlayers(world);

    // Despawn runs regardless of attempts so stragglers get cleaned up even
    // when caps are full.
    despawnFarMobs(world, config, players);

    if (players.empty()) return;

    std::unordered_map<MobCategory, int> counts = countMobs(world);

    for (const auto& [category, rule] : config.categories)
    {
        if (rule.cap <= 0) continue;

        float& timer = timers[category];
        timer += dt;
        if (timer < rule.spawn_interval) continue;
        timer = 0.0f;

        if (counts[category] >= rule.cap) continue;

        for (UUID player_id : players)
        {
            if (counts[category] >= rule.cap) break;
            tryPackSpawn(world, config, category, rule, counts, player_id);
        }
    }
}
