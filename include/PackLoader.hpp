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
    std::string              id;        // "blockbit"
    std::string              name;      // display
    std::string              version;   // semver-ish
    uint32_t                 format = 1;
    std::vector<std::string> depends;   // other pack ids this one needs before load
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
