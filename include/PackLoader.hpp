#ifndef PACK_LOADER_HPP
#define PACK_LOADER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "AssetManager.hpp"
#include "Block.hpp"
#include "Climate.hpp"
#include "Item.hpp"
#include "MobSpawnConfig.hpp"
#include "Registry.hpp"

// Loads a content pack from disk: scans <dir>/pack.json for metadata, then
// <dir>/blocks/*.json and <dir>/items/*.json for content. Each file is a
// single entry keyed by its "id" field (e.g. "blockbit:stone"). Produces
// BlockData / ItemData suitable for inserting into blockDatabase and
// itemDatabase and registering with GameRegistries.
namespace PackLoader
{

struct PackManifest
{
    std::string              id;          // "blockbit"
    std::string              name;        // display
    std::string              version;     // semver-ish
    std::string              description; // free-form blurb for the menu
    uint32_t                 format = 1;
    std::vector<std::string> depends;     // other pack ids this one needs before load
};

// FNV1a-64 fingerprint of pack.json + every content file (sorted by path).
// Not cryptographic — purpose is to detect "same pack version on both sides
// but different files" during the handshake, which commonly happens when a
// modder iterates without bumping the manifest version. 0 means unhashed.
using PackHash = uint64_t;

struct LoadedBlock
{
    RegistryKey key;                           // "blockbit:stone"
    BlockID     legacy_id     = BlockID{};     // valid only when has_legacy_id
    bool        has_legacy_id = false;         // JSON explicitly set legacy_id
    BlockData   data;

    // String-form refs collected during parse; resolved after every pack is
    // installed so cross-pack references work (modpack block dropping a
    // vanilla item, vanilla block resolving a modpack block as support, etc).
    std::string              drop_item;              // -> data.drop->drop
    std::string              drop_alternative_item;  // -> data.drop->alternative_drop
    std::vector<std::string> support_blocks_below;   // -> data.support_blocks_below
    std::string              plant_grows_as;         // -> PlantGrowthParams::grows_as
};

struct LoadedItem
{
    RegistryKey key;
    ItemID      legacy_id     = ItemID{};
    bool        has_legacy_id = false;
    ItemData    data;

    std::string fluid_bucket_fluid;           // -> FluidBucketParams::fluid
    std::string fluid_bucket_empty_variant;   // -> FluidBucketParams::empty_variant
    std::string fluid_bucket_filled_variant;  // -> FluidBucketParams::filled_variant
};

struct LoadResult
{
    bool                     ok = false;
    std::string              error;   // populated when ok == false
    PackManifest             manifest;
    PackHash                 hash = 0; // fingerprint of the on-disk pack files
    std::vector<LoadedBlock> blocks;
    std::vector<LoadedItem>  items;
};

// Full load. A failed load returns `ok=false` with `error` describing the
// first failure — partial state in `blocks`/`items` is not applied. All
// content keys must begin with "<manifest.id>:" or the load is rejected.
// `next_texture_id` is advanced for each PNG file discovered under the
// pack's textures/ directory; AssetManager::loadGameTexture is called
// before block/item JSON parsing so cross-references resolve.
LoadResult loadPack(const std::filesystem::path& dir,
                    AssetManager::GameTextureID& next_texture_id);

struct WorldPackCheck
{
    bool        ok = true;
    std::string reason;   // human-readable on failure, empty on success
};

// Reads <worldDir>/packs.json and validates that every listed pack is
// currently loaded with a matching id/hash. If packs.json is missing, the
// world predates Phase C+D and is accepted as-is. A failure returns ok=false
// with a reason describing the first mismatch found.
WorldPackCheck validateWorldPacks(const std::filesystem::path& worldDir);

// Reads one pack's <packDir>/structures/*.json files and registers each
// described structure template (loaded from a sibling .bbt file) with
// StructureRegistry. The JSON shape:
//
//   { "id": "blockbit:buried_template",
//     "template": "buried_template.bbt",
//     "rule": {
//       "grid": 1,
//       "spawn_chance": 0.0002,
//       "anchor_mode": "below_surface",  // "surface" | "fixed" | "below_surface"
//       "min_surface_y": 0, "max_surface_y": 255,
//       "target_y": 0, "allow_mirror": true,
//       "biomes": ["plains", "forest"]   // omit or [] = any biome
//     }
//   }
//
// Namespace scoping is enforced on "id". The structure's RuntimeID in the
// registry is structureIdFromKey(id) — same across platforms, so world-gen
// placement RNG stays deterministic.
std::size_t scanPackStructures(const std::filesystem::path& packDir,
                               const std::string&           packNamespace);

// Reads one pack's <packDir>/recipes/*.json files and merges them into the
// global `recipes`, `smeltingRecipes`, `fuelBurnTimes` maps declared in
// include/Item.hpp. Each file carries a "type" discriminator:
//
//   { "id": "blockbit:craft_oak_planks",
//     "type": "crafting",
//     "output":       {"item": "blockbit:oak_planks", "quantity": 4},
//     "ingredients":  [{"item": "blockbit:oak_log", "quantity": 1}],
//     "requires_crafting_table": false }
//
//   { "id": "blockbit:smelt_cobblestone",
//     "type": "smelting",
//     "input":  "blockbit:cobblestone",
//     "output": "blockbit:stone",
//     "cook_time": 10.0 }
//
//   { "id": "blockbit:fuel_coal",
//     "type": "fuel",
//     "item":      "blockbit:coal",
//     "burn_time": 80.0 }
//
// Must run after item parsing + resolution so RegistryKey refs land on live
// ItemIDs. Namespace scoping is enforced on the "id" field.
std::size_t scanPackRecipes(const std::filesystem::path& packDir,
                            const std::string&           packNamespace);

// Reads one pack's <packDir>/spawn_rules/*.json files and appends matching
// SpawnEntry records into the given MobSpawnConfig. Each JSON names one
// prefab + biome list; the entry is cloned per-biome. Namespace scoping is
// enforced on the file "id" (if present); allowed_ground keys are stored as
// strings and resolved at check time by MobSpawnSystem.
std::size_t scanPackSpawnRules(const std::filesystem::path& packDir,
                               const std::string&           packNamespace,
                               MobSpawnConfig&              out);

// Recursively scans <packDir>/textures/**/*.png and loads each file into
// AssetManager with a dynamic GameTextureID allocated from the counter.
// Registers "<ns>:<relative-path-sans-extension>" in the texture key map
// so JSON references like {"texture": "ruby_mod:blocks/ruby_ore"} resolve.
// Returns the number of textures loaded. `next_texture_id` is advanced.
std::size_t scanPackTextures(const std::filesystem::path& packDir,
                             const std::string& packNamespace,
                             AssetManager::GameTextureID& next_texture_id);

} // namespace PackLoader

#endif // PACK_LOADER_HPP
