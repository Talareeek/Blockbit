#ifndef GAME_REGISTRIES_HPP
#define GAME_REGISTRIES_HPP

#include "Registry.hpp"
#include "Block.hpp"
#include "Item.hpp"

#include <string>
#include <vector>

// Short summary of a pack that the server loaded. Advertised to clients in
// the handshake so they can reject on missing / mismatched content.
struct LoadedPackSummary
{
    std::string id;
    std::string version;
    uint64_t    hash = 0;
};

// Process-wide registries populated at startup. The blocks/items maps are
// the authoritative source of vanilla and modded content; loaded_packs lists
// every pack directory that contributed, including vanilla.
struct GameRegistries
{
    Registry<BlockData>            blocks;
    Registry<ItemData>             items;
    std::vector<LoadedPackSummary> loaded_packs;
};

GameRegistries& gameRegistries();

// Called once from Game::Game() after asset loading. Walks the hardcoded
// blockDatabase / itemDatabase and registers every entry under the "blockbit"
// namespace with RuntimeID = the entry's legacy enum value.
void registerVanillaContent();

#endif // GAME_REGISTRIES_HPP
