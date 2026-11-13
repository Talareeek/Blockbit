#include "../include/GameRegistries.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/AssetManager.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/GameCommon.hpp"
#include "../include/PackLoader.hpp"
#include "../include/Prefab.hpp"
#include "../include/PrefabLoader.hpp"

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

// Resolve the string refs captured during JSON parse into concrete BlockID
// / ItemID values. Runs after every pack is installed so cross-pack refs
// (vanilla block dropping a modpack item, modpack block whose grows_as is
// a vanilla block) resolve through the fully-populated registry.
void resolveRefs(const PackLoader::LoadResult& pack)
{
    auto& reg = gameRegistries();

    auto resolveItem = [&](const std::string& key,
                           const std::string& context) -> ItemID
    {
        if (key.empty()) return ItemID{};
        if (auto rid = reg.items.find(key); rid.has_value())
            return static_cast<ItemID>(*rid);
        std::cerr << "[Pack] unresolved item ref '" << key
                  << "' in " << context << " — falling back to air\n";
        return ItemID{};
    };

    auto resolveBlock = [&](const std::string& key,
                            const std::string& context) -> BlockID
    {
        if (key.empty()) return BlockID{};
        if (auto rid = reg.blocks.find(key); rid.has_value())
            return static_cast<BlockID>(*rid);
        std::cerr << "[Pack] unresolved block ref '" << key
                  << "' in " << context << " — falling back to air\n";
        return BlockID{};
    };

    for (const auto& lb : pack.blocks)
    {
        auto it = blockDatabase.find(lb.legacy_id);
        if (it == blockDatabase.end()) continue;
        auto& bd = it->second;

        // Skip when the loaded entry didn't carry a string ref — a
        // bootstrap path (legacy numeric) may have already populated the
        // value; overwriting with the empty-string fallback would clobber it.
        if (bd.drop.has_value())
        {
            if (!lb.drop_item.empty())
                bd.drop->drop = resolveItem(lb.drop_item, lb.key + ".drop.item");
            if (!lb.drop_alternative_item.empty())
                bd.drop->alternative_drop = resolveItem(lb.drop_alternative_item, lb.key + ".drop.alternative_item");
        }

        if (!lb.support_blocks_below.empty())
        {
            bd.support_blocks_below.clear();
            bd.support_blocks_below.reserve(lb.support_blocks_below.size());
            for (const auto& key : lb.support_blocks_below)
                bd.support_blocks_below.push_back(resolveBlock(key, lb.key + ".support_blocks_below"));
        }

        if (bd.behavior.type == BlockBehaviorType::PlantGrowth && !lb.plant_grows_as.empty())
        {
            if (auto* p = std::get_if<PlantGrowthParams>(&bd.behavior.params))
                p->grows_as = resolveBlock(lb.plant_grows_as, lb.key + ".behavior.grows_as");
        }
    }

    for (const auto& li : pack.items)
    {
        auto it = itemDatabase.find(li.legacy_id);
        if (it == itemDatabase.end()) continue;
        auto& id = it->second;

        if (id.behavior.type == ItemBehaviorType::FluidBucket)
        {
            if (auto* p = std::get_if<FluidBucketParams>(&id.behavior.params))
            {
                if (!li.fluid_bucket_fluid.empty())
                    p->fluid = resolveBlock(li.fluid_bucket_fluid, li.key + ".behavior.fluid");
                if (!li.fluid_bucket_empty_variant.empty())
                    p->empty_variant = resolveItem(li.fluid_bucket_empty_variant, li.key + ".behavior.empty_variant");
                if (!li.fluid_bucket_filled_variant.empty())
                    p->filled_variant = resolveItem(li.fluid_bucket_filled_variant, li.key + ".behavior.filled_variant");
            }
        }
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

    AssetManager::GameTextureID next_texture = ContentProfiles::firstDynamicTextureId();

    // --- vanilla ----------------------------------------------------------
    auto vanilla = PackLoader::loadPack("resources/packs/blockbit", next_texture);
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

            auto mod = PackLoader::loadPack(entry.path(), next_texture);
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

    // Seed the live spawn table with vanilla defaults — modpack
    // spawn_rules/*.json files append on top.
    reg.spawn_config = defaultMobSpawnConfig();

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
    {
        LoadedPackSummary s{vanilla.manifest.id, vanilla.manifest.version,
                            vanilla.manifest.description, vanilla.hash, {}};
        const std::filesystem::path icon = std::filesystem::path("resources/packs/blockbit") / "icon.png";
        if (std::filesystem::exists(icon)) s.icon_path = std::filesystem::absolute(icon).string();
        reg.loaded_packs.push_back(std::move(s));
    }

    modpacks = topoSortModpacks(std::move(modpacks));
    for (const auto& mod : modpacks)
    {
        std::cout << "[Pack] Installing '" << mod.manifest.id << "'\n";
        installPack(mod, next_block, next_item);
        installPrefabs(packs_root / mod.manifest.id, mod.manifest.id);
        if (const auto added = PackLoader::scanPackSpawnRules(
                packs_root / mod.manifest.id, mod.manifest.id, reg.spawn_config))
        {
            std::cout << "[Pack] '" << mod.manifest.id
                      << "' contributed " << added << " spawn entr(y|ies)\n";
        }

        LoadedPackSummary s{mod.manifest.id, mod.manifest.version,
                            mod.manifest.description, mod.hash, {}};
        const std::filesystem::path icon = packs_root / mod.manifest.id / "icon.png";
        if (std::filesystem::exists(icon)) s.icon_path = std::filesystem::absolute(icon).string();
        reg.loaded_packs.push_back(std::move(s));
    }

    // Every pack is now registered — resolve string cross-refs (drops,
    // support blocks, plant_growth target, fluid-bucket variants) against
    // the fully-populated registry.
    resolveRefs(vanilla);
    for (const auto& mod : modpacks) resolveRefs(mod);

    std::cout << "[Pack] " << reg.loaded_packs.size() << " packs active — "
              << reg.blocks.size() << " blocks, "
              << reg.items.size()  << " items total\n";
}
