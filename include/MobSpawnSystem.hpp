#ifndef MOB_SPAWN_SYSTEM_HPP
#define MOB_SPAWN_SYSTEM_HPP

#include "MobSpawnConfig.hpp"

class ServerWorld;

// Default config accessor — returns the live (vanilla + modpack) spawn
// table from gameRegistries().spawn_config. Overloaded so headers that
// depend on MobSpawnSystem don't need to pull in GameRegistries.
const MobSpawnConfig& liveMobSpawnConfig();

// Natural mob spawning: per-category timers, ring sampling around players,
// weighted species pick per biome, pack spawning with triangular offsets,
// surface/light/collision validation, and distance-based despawn.
//
// Only decides when/where/how many — the actual entity construction is
// delegated to ServerWorld::spawn(RegistryKey, position).
void MobSpawnSystem(ServerWorld& world, float dt, const MobSpawnConfig& config = liveMobSpawnConfig());

#endif // MOB_SPAWN_SYSTEM_HPP
