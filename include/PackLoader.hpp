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

// FNV1a-64 fingerprint of pack.json + every content file (sorted by path).
// Not cryptographic — purpose is to detect "same pack version on both sides
// but different files" during the handshake, which commonly happens when a
// modder iterates without bumping the manifest version. 0 means unhashed.
using PackHash = uint64_t;

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
    PackHash                 hash = 0; // fingerprint of the on-disk pack files
    std::vector<LoadedBlock> blocks;
    std::vector<LoadedItem>  items;
};

// Full load. A failed load returns `ok=false` with `error` describing the
// first failure — partial state in `blocks`/`items` is not applied. All
// content keys must begin with "<manifest.id>:" or the load is rejected.
LoadResult loadPack(const std::filesystem::path& dir);

// Loads every directory in order, returning one LoadResult per pack. A
// failure in any pack aborts with that pack's error in the aggregate's
// first failing entry; preceding successful loads remain in the vector so
// the caller can see what did load.
struct LoadAllResult
{
    bool                    ok = true;
    std::string             error;          // populated when ok == false
    std::size_t             failing_index = 0;
    std::vector<LoadResult> packs;
};
LoadAllResult loadAll(const std::vector<std::filesystem::path>& dirs);

// Writes the loaded entries into blockDatabase and itemDatabase keyed by
// their legacy_id. Overwrites existing entries. Logs a count summary.
void applyToDatabases(const LoadResult& r);

} // namespace PackLoader

#endif // PACK_LOADER_HPP
