#include "../include/GameRegistries.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/ContentProfiles.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PackLoader.hpp"
#include "../include/Prefab.hpp"
#include "../include/PrefabLoader.hpp"
#include "../include/VanillaExporter.hpp"

GameRegistries& gameRegistries()
{
    static GameRegistries instance;
    return instance;
}

namespace
{

// Walk a pack's entries and install them into blockDatabase/itemDatabase and
// the live registries. Vanilla entries carry an explicit `legacy_id` from
// JSON — those are used as-is. Modpack entries don't; they're assigned the
// next unused RuntimeID. `next_block` / `next_item` track the high-water
// mark and grow monotonically so later packs never collide with earlier ones.
void installPack(const PackLoader::LoadResult& pack,
                 RuntimeID& next_block,
                 RuntimeID& next_item)
{
    auto& reg = gameRegistries();

    for (const auto& lb : pack.blocks)
    {
        const BlockID id = lb.has_legacy_id ? lb.legacy_id
                                             : static_cast<BlockID>(next_block);
        const RuntimeID rid = static_cast<RuntimeID>(id);

        blockDatabase[id] = lb.data;
        reg.blocks.registerAt(rid, lb.key, lb.data);

        next_block = std::max<RuntimeID>(next_block, static_cast<RuntimeID>(rid + 1));
    }

    for (const auto& li : pack.items)
    {
        const ItemID id = li.has_legacy_id ? li.legacy_id
                                            : static_cast<ItemID>(next_item);
        const RuntimeID rid = static_cast<RuntimeID>(id);

        itemDatabase[id] = li.data;
        reg.items.registerAt(rid, li.key, li.data);

        next_item = std::max<RuntimeID>(next_item, static_cast<RuntimeID>(rid + 1));
    }
}

void installPrefabs(const std::filesystem::path& dir, const std::string& ns)
{
    auto scan = PrefabLoader::scanPrefabs(dir, ns);
    if (!scan.ok)
    {
        std::cerr << "[Pack] prefab load error in '" << ns << "': " << scan.error << '\n';
        return;
    }
    auto& reg = gameRegistries();
    for (auto& lp : scan.prefabs)
    {
        // Entity is move-only; store via the UUID-remapping clone so the
        // registry's template never aliases a live world entity's UUID.
        reg.prefabs.insert_or_assign(lp.key, lp.data.cloneAs(UUID{}));
    }
    if (!scan.prefabs.empty())
        std::cout << "[Pack] '" << ns << "' contributed "
                  << scan.prefabs.size() << " prefab(s)\n";
}

// Kahn's algorithm. Returns a dependency-respecting order over the modpack
// list, or an empty vector with an error message written to std::cerr when
// something is missing / cyclic. Vanilla ('blockbit') is NEVER listed here —
// callers load it first and treat it as a synthetic root. Any modpack
// listing 'blockbit' in its depends is satisfied automatically since vanilla
// is loaded before this returns.
std::vector<PackLoader::LoadResult>
topoSortModpacks(std::vector<PackLoader::LoadResult> packs)
{
    std::unordered_map<std::string, std::size_t> index_by_id;
    for (std::size_t i = 0; i < packs.size(); ++i) index_by_id[packs[i].manifest.id] = i;

    // Count unmet (non-self) deps for each node. "blockbit" is assumed met.
    std::vector<std::size_t> indeg(packs.size(), 0);
    std::vector<std::vector<std::size_t>> adj(packs.size());

    for (std::size_t i = 0; i < packs.size(); ++i)
    {
        for (const auto& dep : packs[i].manifest.depends)
        {
            if (dep == "blockbit") continue;
            auto it = index_by_id.find(dep);
            if (it == index_by_id.end())
            {
                std::cerr << "[Pack] '" << packs[i].manifest.id
                          << "' depends on missing pack '" << dep
                          << "' — skipping the dependent and its dependers.\n";
                return {};
            }
            adj[it->second].push_back(i);
            ++indeg[i];
        }
    }

    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < packs.size(); ++i) if (indeg[i] == 0) ready.push_back(i);

    std::vector<PackLoader::LoadResult> out;
    out.reserve(packs.size());
    while (!ready.empty())
    {
        const auto n = ready.back();
        ready.pop_back();
        out.push_back(std::move(packs[n]));
        for (auto m : adj[n])
            if (--indeg[m] == 0) ready.push_back(m);
    }

    if (out.size() != packs.size())
    {
        std::cerr << "[Pack] dependency cycle detected — "
                  << (packs.size() - out.size())
                  << " pack(s) skipped. Remaining in sort: " << out.size() << ".\n";
    }
    return out;
}

} // namespace

void registerVanillaContent()
{
    auto& reg = gameRegistries();

    // Fresh state — the previous implementation needed a two-pass seed from
    // the static initializers, but those are gone since Phase 1c-E, so we
    // can start from zero every time this function runs.
    reg.blocks.clear();
    reg.items.clear();
    reg.prefabs.clear();
    reg.loaded_packs.clear();
    blockDatabase.clear();
    itemDatabase.clear();

    // --- vanilla ----------------------------------------------------------
    auto vanilla = PackLoader::loadPack("resources/packs/blockbit");
    if (!vanilla.ok)
    {
        std::cerr << "[Pack] FATAL: cannot load vanilla pack: " << vanilla.error << '\n';
        std::cerr << "[Pack] Expected at resources/packs/blockbit/ (relative to CWD).\n";
        std::exit(1);
    }
    std::cout << "[Pack] Loaded '" << vanilla.manifest.id
              << "' v" << vanilla.manifest.version
              << " hash=" << std::hex << vanilla.hash << std::dec
              << " — " << vanilla.blocks.size() << " blocks, "
              << vanilla.items.size() << " items\n";

    // --- modpack discovery ------------------------------------------------
    std::vector<PackLoader::LoadResult> modpacks;
    const std::filesystem::path packs_root = getHomePath() / "Blockbit" / "packs";
    if (std::filesystem::exists(packs_root))
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(packs_root, ec))
        {
            if (!entry.is_directory()) continue;
            if (!std::filesystem::exists(entry.path() / "pack.json")) continue;

            auto mod = PackLoader::loadPack(entry.path());
            if (!mod.ok)
            {
                std::cerr << "[Pack] Skipping " << entry.path()
                          << ": " << mod.error << '\n';
                continue;
            }
            if (mod.manifest.id == "blockbit")
            {
                std::cerr << "[Pack] Skipping " << entry.path()
                          << ": reserved namespace 'blockbit'\n";
                continue;
            }
            std::cout << "[Pack] Loaded '" << mod.manifest.id
                      << "' v" << mod.manifest.version
                      << " hash=" << std::hex << mod.hash << std::dec
                      << " — " << mod.blocks.size() << " blocks, "
                      << mod.items.size() << " items\n";
            modpacks.push_back(std::move(mod));
        }
    }

    // --- install in order: vanilla first so its legacy_ids pin the low
    //     slots, then modpacks grab the next free IDs. --------------------
    RuntimeID next_block = 0;
    RuntimeID next_item  = 0;

    installPack(vanilla, next_block, next_item);

    // Mirror hardcoded vanilla prefabs into the key-based registry so
    // spawn-by-key works for 'blockbit:pig', 'blockbit:player', etc.
    // PrefabRegistry::init runs before registerVanillaContent (see
    // Game::Game / HeadlessMain) — safe to read here.
    for (const auto pid : {PrefabID::Player, PrefabID::Pig, PrefabID::Dynamite,
                           PrefabID::DroppedItem, PrefabID::FallingBlock,
                           PrefabID::Cube})
    {
        const std::string key = "blockbit:" + ContentProfiles::nameFromPrefab(pid);
        reg.prefabs.insert_or_assign(key, PrefabRegistry::get(pid).cloneAs(UUID{}));
    }

    installPrefabs("resources/packs/blockbit", vanilla.manifest.id);
    reg.loaded_packs.push_back({vanilla.manifest.id, vanilla.manifest.version, vanilla.hash});

    modpacks = topoSortModpacks(std::move(modpacks));
    for (const auto& mod : modpacks)
    {
        std::cout << "[Pack] Installing '" << mod.manifest.id << "'\n";
        installPack(mod, next_block, next_item);
        installPrefabs(packs_root / mod.manifest.id, mod.manifest.id);
        reg.loaded_packs.push_back({mod.manifest.id, mod.manifest.version, mod.hash});
    }

    std::cout << "[Pack] " << reg.loaded_packs.size() << " packs active — "
              << reg.blocks.size() << " blocks, "
              << reg.items.size()  << " items total\n";

    // --- one-shot exporter hook. Runs AFTER everything is loaded so a dump
    //     reflects the active content set (including modpack additions). --
    if (const char* export_dir = std::getenv("BLOCKBIT_EXPORT_VANILLA"))
    {
        auto stats = VanillaExporter::exportTo(export_dir);
        std::cout << "[VanillaExporter] Wrote "
                  << stats.blocks_written << " blocks, "
                  << stats.items_written  << " items to " << export_dir << '\n';
        std::exit(0);
    }
}
