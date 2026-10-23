#ifndef MOB_SPAWN_SYSTEM_HPP
#define MOB_SPAWN_SYSTEM_HPP

#include "MobSpawnConfig.hpp"

class ServerWorld;

// Natural mob spawning: per-category timers, ring sampling around players,
// weighted species pick per biome, pack spawning with triangular offsets,
// surface/light/collision validation, and distance-based despawn.
//
// Only decides when/where/how many — the actual entity construction is
// delegated to ServerWorld::spawn(PrefabID, position).
void MobSpawnSystem(ServerWorld& world, float dt, const MobSpawnConfig& config = defaultMobSpawnConfig());

#endif // MOB_SPAWN_SYSTEM_HPP
