#ifndef GAME_REGISTRIES_HPP
#define GAME_REGISTRIES_HPP

#include "Registry.hpp"
#include "Block.hpp"
#include "Item.hpp"

// Process-wide registries populated at startup. During Phase 1a-A these
// mirror the legacy blockDatabase / itemDatabase: the RuntimeID handed back
// equals the old enum value, so nothing downstream shifts. Later phases
// (chunk palette, RegistrySyncPacket) start reading from here instead of the
// legacy maps.
struct GameRegistries
{
    Registry<BlockData> blocks;
    Registry<ItemData>  items;
};

GameRegistries& gameRegistries();

// Called once from Game::Game() after asset loading. Walks the hardcoded
// blockDatabase / itemDatabase and registers every entry under the "blockbit"
// namespace with RuntimeID = the entry's legacy enum value.
void registerVanillaContent();

#endif // GAME_REGISTRIES_HPP
