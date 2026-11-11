#ifndef PACK_LOADER_HPP
#define PACK_LOADER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "AssetManager.hpp"
#include "Block.hpp"
#include "Item.hpp"
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
};

struct LoadedItem
{
    RegistryKey key;
    ItemID      legacy_id     = ItemID{};
    bool        has_legacy_id = false;
    ItemData    data;
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
