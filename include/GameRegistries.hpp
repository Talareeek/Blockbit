#ifndef GAME_REGISTRIES_HPP
#define GAME_REGISTRIES_HPP

#include "Registry.hpp"
#include "Block.hpp"
#include "Entity.hpp"
#include "Item.hpp"
#include "MobSpawnConfig.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// Short summary of a pack that the server loaded. Advertised to clients in
// the handshake so they can reject on missing / mismatched content.
struct LoadedPackSummary
{
    std::string id;
    std::string version;
    uint64_t    hash = 0;
    // Absolute path to <pack>/icon.png if present; empty when the pack
    // didn't ship an icon. UI layers load sf::Texture lazily from this
    // path — keeps the pack system free of SFML coupling.
    std::string icon_path;
};

// Process-wide registries populated at startup. The blocks/items maps are
// the authoritative source of vanilla and modded content; loaded_packs lists
// every pack directory that contributed, including vanilla.
struct GameRegistries
{
    Registry<BlockData>            blocks;
    Registry<ItemData>             items;
    std::vector<LoadedPackSummary> loaded_packs;

    // Prefab templates keyed by RegistryKey (e.g. "blockbit:pig",
    // "ruby_mod:rubyling"). Vanilla prefabs are mirrored here after
    // PrefabRegistry::init; modpack prefabs are added by the pack loader.
    // The stored Entity is a template — spawn helpers deep-copy it, assign
    // a fresh UUID, and inject the spawn position before adding to World.
    std::unordered_map<std::string, Entity> prefabs;

    // Live biome spawn table. Seeded from defaultMobSpawnConfig() and
    // augmented by each pack's spawn_rules/*.json. Consumed by MobSpawnSystem.
    MobSpawnConfig spawn_config;
};

GameRegistries& gameRegistries();

// Called once from Game::Game() after asset loading. Walks the hardcoded
// blockDatabase / itemDatabase and registers every entry under the "blockbit"
// namespace with RuntimeID = the entry's legacy enum value.
void registerVanillaContent();

#endif // GAME_REGISTRIES_HPP
