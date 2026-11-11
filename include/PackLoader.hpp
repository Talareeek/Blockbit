#ifndef PACK_LOADER_HPP
#define PACK_LOADER_HPP

#include <filesystem>
#include <string>
#include <vector>

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
    std::string id;        // "blockbit"
    std::string name;      // display
    std::string version;   // semver-ish
    uint32_t    format = 1;
};

struct LoadedBlock
{
    RegistryKey key;       // "blockbit:stone"
    BlockID     legacy_id; // optional override, 0 if unspecified — see schema
    BlockData   data;
};

struct LoadedItem
{
    RegistryKey key;
    ItemID      legacy_id;
    ItemData    data;
};

struct LoadResult
{
    bool                     ok = false;
    std::string              error;   // populated when ok == false
    PackManifest             manifest;
    std::vector<LoadedBlock> blocks;
    std::vector<LoadedItem>  items;
};

// Full load. A failed load returns `ok=false` with `error` describing the
// first failure — partial state in `blocks`/`items` is not applied.
LoadResult loadPack(const std::filesystem::path& dir);

// Writes the loaded entries into blockDatabase and itemDatabase keyed by
// their legacy_id. Overwrites existing entries, letting the pack become the
// authoritative source while the hardcoded static initializers still run as
// a safety net. Logs a count summary.
void applyToDatabases(const LoadResult& r);

} // namespace PackLoader

#endif // PACK_LOADER_HPP
