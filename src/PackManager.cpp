#include "../include/PackManager.hpp"
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
#include "../include/Item.hpp"
#include "../include/Prefab.hpp"
#include "../include/PrefabLoader.hpp"

namespace
{

// Install entries under their canonical names. RuntimeIDs are an internal
// lookup detail; world data and wire payloads carry these full names.
void installPack(const PackLoader::LoadResult& pack)
{
    auto& reg = gameRegistries();

    for (const auto& lb : pack.blocks)
    {
        blockDatabase[lb.key] = lb.data;
        reg.blocks.registerEntry(lb.key, lb.data);
    }

    for (const auto& li : pack.items)
    {
        itemDatabase[li.key] = li.data;
        reg.items.registerEntry(li.key, li.data);
    }
}

// Resolve the string refs captured during JSON parse into concrete BlockKey
// / ItemKey values. Runs after every pack is installed so cross-pack refs
// (vanilla block dropping a modpack item, modpack block whose grows_as is
// a vanilla block) resolve through the fully-populated registry.
void resolveRefs(const PackLoader::LoadResult& pack)
{
    auto& reg = gameRegistries();

    auto resolveItem = [&](const std::string& key,
                           const std::string& context) -> ItemKey
    {
        if (key.empty()) return ItemKey{};
        if (reg.items.find(key).has_value()) return key;
        std::cerr << "[Pack] unresolved item ref '" << key
                  << "' in " << context << " — falling back to air\n";
        return ItemKey{};
    };

    auto resolveBlock = [&](const std::string& key,
                            const std::string& context) -> BlockKey
    {
        if (key.empty()) return BlockKey{};
        if (reg.blocks.find(key).has_value()) return key;
        std::cerr << "[Pack] unresolved block ref '" << key
                  << "' in " << context << " — falling back to air\n";
        return BlockKey{};
    };

    for (const auto& lb : pack.blocks)
    {
        auto it = blockDatabase.find(lb.key);
        if (it == blockDatabase.end()) continue;
        auto& bd = it->second;

        // Empty string refs mean the corresponding behavior is not configured.
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
        auto it = itemDatabase.find(li.key);
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

void PackManager::loadAllContent()
{
    auto& reg = gameRegistries();

    reg.blocks.clear();
    reg.items.clear();
    reg.textures.clear();
    reg.prefabs.clear();
    reg.loaded_packs.clear();
    blockDatabase.clear();
    itemDatabase.clear();

    RuntimeID next_texture = ContentProfiles::firstDynamicTextureId();

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
        std::vector<std::filesystem::path> pack_dirs;
        for (const auto& entry : std::filesystem::directory_iterator(packs_root, ec))
        {
            if (!entry.is_directory()) continue;
            if (!std::filesystem::exists(entry.path() / "pack.json")) continue;
            pack_dirs.push_back(entry.path());
        }
        std::sort(pack_dirs.begin(), pack_dirs.end());

        for (const auto& pack_dir : pack_dirs)
        {

            auto mod = PackLoader::loadPack(pack_dir, next_texture);
            if (!mod.ok)
            {
                std::cerr << "[Pack] Skipping " << pack_dir
                          << ": " << mod.error << '\n';
                continue;
            }
            if (mod.manifest.id == "blockbit")
            {
                std::cerr << "[Pack] Skipping " << pack_dir
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

    // --- install vanilla first, then modpacks in dependency order. -------
    installPack(vanilla);
    // Prefabs with render components need texture IDs registered by the pack
    // scan above. Load them before block behavior references are used.
    installPrefabs("resources/packs/blockbit", vanilla.manifest.id);

    // Seed the live spawn table with vanilla defaults — modpack
    // spawn_rules/*.json files append on top.
    reg.spawn_config = defaultMobSpawnConfig();

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
        installPack(mod);
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

    // Recipes rely on item-key resolution, so they load after resolveRefs.
    recipes.clear();
    smeltingRecipes.clear();
    fuelBurnTimes.clear();
    {
        const auto added = PackLoader::scanPackRecipes("resources/packs/blockbit",
                                                       vanilla.manifest.id);
        if (added > 0)
            std::cout << "[Pack] 'blockbit' loaded " << added << " recipe(s)\n";
    }
    for (const auto& mod : modpacks)
    {
        const auto added = PackLoader::scanPackRecipes(packs_root / mod.manifest.id,
                                                       mod.manifest.id);
        if (added > 0)
            std::cout << "[Pack] '" << mod.manifest.id << "' loaded "
                      << added << " recipe(s)\n";
    }

    // Structures — loaded last so modpacks referencing vanilla biomes/blocks
    // work. Templates stay in their original .bbt format; only the placement
    // rule and the pointer-to-.bbt live in JSON.
    {
        const auto added = PackLoader::scanPackStructures("resources/packs/blockbit",
                                                          vanilla.manifest.id);
        if (added > 0)
            std::cout << "[Pack] 'blockbit' loaded " << added << " structure(s)\n";
    }
    for (const auto& mod : modpacks)
    {
        const auto added = PackLoader::scanPackStructures(packs_root / mod.manifest.id,
                                                          mod.manifest.id);
        if (added > 0)
            std::cout << "[Pack] '" << mod.manifest.id << "' loaded "
                      << added << " structure(s)\n";
    }

    // Loot tables + jigsaw pools follow structures. Pool entries resolve to
    // Structure keys resolve when chain expansion runs; if a modpack declares
    // a pool before its structures are registered, it simply yields no match at
    // runtime instead of crashing.
    {
        const auto added = PackLoader::scanPackLootTables("resources/packs/blockbit",
                                                          vanilla.manifest.id);
        if (added > 0)
            std::cout << "[Pack] 'blockbit' loaded " << added << " loot table(s)\n";
    }
    for (const auto& mod : modpacks)
    {
        const auto added = PackLoader::scanPackLootTables(packs_root / mod.manifest.id,
                                                          mod.manifest.id);
        if (added > 0)
            std::cout << "[Pack] '" << mod.manifest.id << "' loaded "
                      << added << " loot table(s)\n";
    }
    {
        const auto added = PackLoader::scanPackJigsawPools("resources/packs/blockbit",
                                                           vanilla.manifest.id);
        if (added > 0)
            std::cout << "[Pack] 'blockbit' loaded " << added << " jigsaw pool(s)\n";
    }
    for (const auto& mod : modpacks)
    {
        const auto added = PackLoader::scanPackJigsawPools(packs_root / mod.manifest.id,
                                                           mod.manifest.id);
        if (added > 0)
            std::cout << "[Pack] '" << mod.manifest.id << "' loaded "
                      << added << " jigsaw pool(s)\n";
    }

    std::cout << "[Pack] " << reg.loaded_packs.size() << " packs active — "
              << reg.blocks.size() << " blocks, "
              << reg.items.size()  << " items total\n";
}


// Pack file parsing and auxiliary content scanners.
#include "../include/PackManager.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../include/AssetManager.hpp"
#include "../include/BBT.hpp"
#include "../include/Behavior.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/GameRegistries.hpp"
#include "../include/Registry.hpp"
#include "../include/StructureRegistry.hpp"
#include "../include/StructureTemplate.hpp"

namespace PackLoader
{

namespace
{

using json = nlohmann::json;

// Parses "behavior" object for an item. Returns true on success (including
// type == "none" which yields an empty behavior). On any error, leaves `out`
// at ItemBehaviorType::None and sets `err`.
bool parseItemBehaviorJson(const nlohmann::json& j, ItemBehavior& out, std::string& err)
{
    const std::string type_name = j.value("type", "none");
    auto type = ContentProfiles::itemBehaviorTypeFromName(type_name);
    if (!type.has_value())
    {
        err = "unknown item behavior type '" + type_name + "'";
        return false;
    }

    out.type = *type;
    const auto& params = j.contains("params") ? j["params"] : nlohmann::json::object();

    switch (out.type)
    {
        case ItemBehaviorType::None:
            return true;

        case ItemBehaviorType::ThrowProjectile:
        {
            ThrowProjectileParams p{};
            p.projectile = params.value("projectile", std::string{});
            if (p.projectile.empty())
            { err = "throw_projectile: missing 'projectile' key"; return false; }

            const std::string pattern_name = params.value("pattern", "single");
            if (auto pat = ContentProfiles::throwPatternFromName(pattern_name); pat.has_value())
                p.pattern = *pat;
            else { err = "unknown throw pattern '" + pattern_name + "'"; return false; }

            p.count         = params.value("count", 1u);
            p.throw_speed   = params.value("throw_speed", 0.0f);
            p.spread        = params.value("spread", 0.0f);
            p.height_offset = params.value("height_offset", 0.0f);
            out.params = p;
            return true;
        }

        case ItemBehaviorType::FluidBucket:
        {
            FluidBucketParams p{};
            p.fluid              = BlockKey{};  // patched during resolution
            p.source_level_value = static_cast<uint8_t>(params.value("source_level_value", 0));
            p.empty_variant      = ItemKey{};
            p.filled_variant     = ItemKey{};
            out.params = p;
            return true;
        }

        case ItemBehaviorType::Ignite:
        {
            IgniteParams p{};
            p.consume_on_success = params.value("consume_on_success", false);
            p.can_place_fire     = params.value("can_place_fire",     false);
            out.params = p;
            return true;
        }

        case ItemBehaviorType::ConsumeEffect:
            out.params = ConsumeEffectParams{};
            return true;
    }
    return false;
}

bool parseBlockBehaviorJson(const nlohmann::json& j, BlockBehavior& out, std::string& err)
{
    const std::string type_name = j.value("type", "none");
    auto type = ContentProfiles::blockBehaviorTypeFromName(type_name);
    if (!type.has_value())
    {
        err = "unknown block behavior type '" + type_name + "'";
        return false;
    }

    out.type = *type;
    const auto& params = j.contains("params") ? j["params"] : nlohmann::json::object();

    switch (out.type)
    {
        case BlockBehaviorType::None:
            return true;

        case BlockBehaviorType::CascadeFluid:
        {
            CascadeFluidParams p{};
            p.max_level        = static_cast<uint8_t>(params.value("max_level", 0));
            p.tick_delay_ticks = params.value("tick_delay_ticks", 0u);
            out.params = p;
            return true;
        }

        case BlockBehaviorType::GravityBlock:
        {
            GravityBlockParams p{};
            p.fall_delay_ticks = params.value("fall_delay_ticks", 0u);
            const std::string prefab_name = params.value("falling_prefab", "blockbit:falling_block");
            p.falling_prefab = prefab_name.find(':') == std::string::npos
                ? "blockbit:" + prefab_name
                : prefab_name;
            out.params = p;
            return true;
        }

        case BlockBehaviorType::PlantGrowth:
        {
            PlantGrowthParams p{};
            p.max_height = static_cast<uint8_t>(params.value("max_height", 1));
            p.grows_as   = BlockKey{}; // patched during resolution
            out.params = p;
            return true;
        }
    }
    return false;
}

// FNV1a-64 — fast, deterministic, good enough for "did this file change?"
// Not for security. Seeded with the official FNV offset basis.
constexpr uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime  = 1099511628211ULL;

uint64_t fnvFold(uint64_t h, const char* data, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        h ^= static_cast<uint8_t>(data[i]);
        h *= kFnvPrime;
    }
    return h;
}

PackHash computePackHash(const std::filesystem::path& dir)
{
    uint64_t h = kFnvOffset;

    // Collect every regular file under dir sorted by path so the hash is
    // stable regardless of filesystem iteration order.
    std::vector<std::filesystem::path> files;
    for (auto it  = std::filesystem::recursive_directory_iterator(dir);
              it != std::filesystem::recursive_directory_iterator(); ++it)
    {
        if (it->is_regular_file()) files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& f : files)
    {
        // Fold in the relative path first so renaming a file changes the hash.
        const auto rel = std::filesystem::relative(f, dir).generic_string();
        h = fnvFold(h, rel.data(), rel.size());
        h = fnvFold(h, "\0", 1);

        std::ifstream in(f, std::ios::binary);
        if (!in) continue;
        char buf[4096];
        while (in)
        {
            in.read(buf, sizeof(buf));
            const auto got = in.gcount();
            if (got > 0) h = fnvFold(h, buf, static_cast<std::size_t>(got));
        }
        h = fnvFold(h, "\0", 1);
    }
    return h;
}

std::string readFile(const std::filesystem::path& p)
{
    std::ifstream f(p);
    if (!f) return {};
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// -------- Block JSON → BlockData -----------------------------------------

bool parseBlock(const std::filesystem::path& file, LoadedBlock& out, std::string& err)
{
    const std::string raw = readFile(file);
    if (raw.empty())
    {
        err = "cannot read " + file.string();
        return false;
    }

    json j;
    try { j = json::parse(raw); }
    catch (const std::exception& e)
    {
        err = file.string() + ": " + e.what();
        return false;
    }

    if (!j.contains("id") || !j["id"].is_string())
    {
        err = file.string() + ": missing 'id'";
        return false;
    }

    out.key = j["id"].get<std::string>();
    if (!isValidRegistryKey(out.key))
    {
        err = file.string() + ": invalid key '" + out.key + "'";
        return false;
    }

    BlockData& b = out.data;
    b.solid       = j.value("solid", false);
    b.transparent = j.value("transparent", true);
    b.breakable   = j.value("breakable", true);
    b.liquid      = j.value("liquid", false);
    b.drag        = j.value("drag", 1.0f);
    b.hardness    = j.value("hardness", 0.0f);
    b.light_emission = static_cast<uint8_t>(j.value("light_emission", 0));
    b.light_opacity  = static_cast<uint8_t>(j.value("light_opacity", 15));
    b.lets_light     = j.value("lets_light", false);
    b.random_tickable = j.value("random_tickable", false);
    b.pp_accept_mask = static_cast<uint8_t>(j.value("pp_accept_mask", 0x0F));

    if (j.contains("render") && j["render"].is_object())
    {
        const auto& r = j["render"];
        RenderProperties props{};
        if (const auto tex = ContentProfiles::textureFromName(
                r.value("texture", std::string{})); tex.has_value())
        {
            props.texture = *tex;
        }
        else
        {
            err = file.string() + ": unknown texture '" + r.value("texture", std::string{}) + "'";
            return false;
        }
        const std::string profile = r.value("profile", "default");
        if (!ContentProfiles::applyRenderProfile(props, profile))
        {
            err = file.string() + ": unknown render profile '" + profile + "'";
            return false;
        }
        if (r.contains("connects_to"))
        {
            if (!r["connects_to"].is_array())
            {
                err = file.string() + ": render.connects_to must be an array of block IDs";
                return false;
            }
            for (const auto& value : r["connects_to"])
            {
                if (!value.is_string())
                {
                    err = file.string() + ": render.connects_to entries must be block ID strings";
                    return false;
                }
                const std::string key = value.get<std::string>();
                if (!isValidRegistryKey(key))
                {
                    err = file.string() + ": invalid block ID in render.connects_to: '" + key + "'";
                    return false;
                }
                props.connects_to.emplace_back(key);
            }
            if (profile == "connected")
                props.rect = make_connected_render_rect(props.connects_to);
            else if (profile == "ruby_wire")
                props.rect = make_ruby_wire_render_rect(props.connects_to);
        }
        b.render = props;
    }

    if (j.contains("mining") && j["mining"].is_object())
    {
        const auto& m = j["mining"];
        MiningProperties mp{};
        mp.mining_resistance = m.value("resistance", 0);
        const std::string tool_name = m.value("tool", "none");
        auto tool = ContentProfiles::toolTypeFromName(tool_name);
        if (!tool.has_value())
        {
            err = file.string() + ": unknown tool '" + tool_name + "'";
            return false;
        }
        mp.desired_tool = {*tool, m.value("level", 0)};
        b.mining = mp;
    }

    if (j.contains("drop") && j["drop"].is_object())
    {
        const auto& d = j["drop"];
        DropProperties dp{};
        dp.always_drop             = d.value("always", false);
        dp.drop                    = ItemKey{}; // patched during resolution
        dp.min_amount              = static_cast<uint8_t>(d.value("min", 1));
        dp.max_amount              = static_cast<uint8_t>(d.value("max", 1));
        dp.alternative_drop_chance = d.value("alternative_chance", 0.0f);
        dp.alternative_drop        = ItemKey{}; // patched during resolution
        b.drop = dp;
        out.drop_item             = d.value("item",             std::string{});
        out.drop_alternative_item = d.value("alternative_item", std::string{});
    }

    const std::string cb_profile = j.value("callbacks", "none");
    if (!ContentProfiles::applyCallbackProfile(b.callbacks, cb_profile))
    {
        err = file.string() + ": unknown callback profile '" + cb_profile + "'";
        return false;
    }

    // support_blocks_below carries RegistryKey strings. The BlockData's
    // support_blocks_below vector is filled during the resolution pass once
    // every pack has registered its blocks.
    if (j.contains("support_blocks_below") && j["support_blocks_below"].is_array())
    {
        for (const auto& v : j["support_blocks_below"])
            if (v.is_string()) out.support_blocks_below.push_back(v.get<std::string>());
    }

    if (j.contains("behavior") && j["behavior"].is_object())
    {
        std::string bh_err;
        if (!parseBlockBehaviorJson(j["behavior"], b.behavior, bh_err))
        {
            err = file.string() + ": behavior: " + bh_err;
            return false;
        }

        // Harvest string refs that the behavior parser left for resolution
        // (currently only plant_growth.grows_as).
        if (b.behavior.type == BlockBehaviorType::PlantGrowth)
        {
            const auto& params = j["behavior"].value("params", nlohmann::json::object());
            out.plant_grows_as = params.value("grows_as", std::string{});
        }
    }

    return true;
}

// -------- Item JSON → ItemData -------------------------------------------

bool parseItem(const std::filesystem::path& file, LoadedItem& out, std::string& err)
{
    const std::string raw = readFile(file);
    if (raw.empty())
    {
        err = "cannot read " + file.string();
        return false;
    }

    json j;
    try { j = json::parse(raw); }
    catch (const std::exception& e)
    {
        err = file.string() + ": " + e.what();
        return false;
    }

    if (!j.contains("id") || !j["id"].is_string())
    {
        err = file.string() + ": missing 'id'";
        return false;
    }

    out.key = j["id"].get<std::string>();
    if (!isValidRegistryKey(out.key))
    {
        err = file.string() + ": invalid key '" + out.key + "'";
        return false;
    }

    ItemData& it = out.data;
    it.name         = j.value("name", std::string{});
    it.maxStackSize = j.value("max_stack_size", 64u);
    it.attack_delay = j.value("attack_delay", 0.25f);
    it.is_bottle    = j.value("is_bottle", false);

    if (j.contains("texture") && j["texture"].is_string())
    {
        if (auto tex = ContentProfiles::textureFromName(j["texture"].get<std::string>()); tex.has_value())
            it.texture = *tex;
        else
        {
            err = file.string() + ": unknown texture '" + j["texture"].get<std::string>() + "'";
            return false;
        }
    }

    const std::string rarity = j.value("rarity", "common");
    if (auto r = ContentProfiles::rarityFromName(rarity); r.has_value())
        it.rarity = *r;
    else { err = file.string() + ": unknown rarity '" + rarity + "'"; return false; }

    const std::string category = j.value("category", "misc");
    if (auto c = ContentProfiles::categoryFromName(category); c.has_value())
        it.category = *c;
    else { err = file.string() + ": unknown category '" + category + "'"; return false; }

    if (j.contains("tool") && j["tool"].is_object())
    {
        const auto& t = j["tool"];
        const std::string tool = t.value("type", "none");
        if (auto tt = ContentProfiles::toolTypeFromName(tool); tt.has_value())
            it.tool = ToolProperties{*tt, t.value("level", 0)};
        else { err = file.string() + ": unknown tool type '" + tool + "'"; return false; }
    }

    if (j.contains("max_durability") && j["max_durability"].is_number_integer())
        it.maxDurability = j["max_durability"].get<uint32_t>();

    if (j.contains("damage") && j["damage"].is_number_integer())
        it.damage = j["damage"].get<uint32_t>();

    if (j.contains("behavior") && j["behavior"].is_object())
    {
        std::string bh_err;
        if (!parseItemBehaviorJson(j["behavior"], it.behavior, bh_err))
        {
            err = file.string() + ": behavior: " + bh_err;
            return false;
        }

        if (it.behavior.type == ItemBehaviorType::FluidBucket)
        {
            const auto& params = j["behavior"].value("params", nlohmann::json::object());
            out.fluid_bucket_fluid          = params.value("fluid",          std::string{});
            out.fluid_bucket_empty_variant  = params.value("empty_variant",  std::string{});
            out.fluid_bucket_filled_variant = params.value("filled_variant", std::string{});
        }
    }

    return true;
}

bool keyInNamespace(const RegistryKey& key, const std::string& ns)
{
    if (ns.empty()) return true; // manifest without id — accept (vanilla case)
    if (key.size() <= ns.size() + 1) return false;
    if (key.compare(0, ns.size(), ns) != 0) return false;
    return key[ns.size()] == ':';
}

bool scanDir(const std::filesystem::path& dir,
             LoadResult& result,
             bool is_block,
             std::string& err)
{
    if (!std::filesystem::exists(dir)) return true;

    const std::string& ns = result.manifest.id;

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        if (is_block)
        {
            LoadedBlock lb;
            if (!parseBlock(entry.path(), lb, err)) return false;
            if (!keyInNamespace(lb.key, ns))
            {
                err = entry.path().string() + ": key '" + lb.key
                    + "' does not belong to pack namespace '" + ns + "'";
                return false;
            }
            result.blocks.push_back(std::move(lb));
        }
        else
        {
            LoadedItem li;
            if (!parseItem(entry.path(), li, err)) return false;
            if (!keyInNamespace(li.key, ns))
            {
                err = entry.path().string() + ": key '" + li.key
                    + "' does not belong to pack namespace '" + ns + "'";
                return false;
            }
            result.items.push_back(std::move(li));
        }
    }
    return true;
}

} // namespace

LoadResult loadPack(const std::filesystem::path& dir,
                    RuntimeID& next_texture_id)
{
    LoadResult result;

    const auto manifest_path = dir / "pack.json";
    const std::string raw = readFile(manifest_path);
    if (raw.empty())
    {
        result.error = "cannot read " + manifest_path.string();
        return result;
    }

    try
    {
        auto j = json::parse(raw);
        result.manifest.id          = j.value("id", std::string{});
        result.manifest.name        = j.value("name", std::string{});
        result.manifest.version     = j.value("version", std::string{});
        result.manifest.description = j.value("description", std::string{});
        result.manifest.format      = j.value("format", 1u);
        if (j.contains("depends") && j["depends"].is_array())
        {
            for (const auto& d : j["depends"])
                if (d.is_string()) result.manifest.depends.push_back(d.get<std::string>());
        }
    }
    catch (const std::exception& e)
    {
        result.error = manifest_path.string() + ": " + e.what();
        return result;
    }

    // Textures must register BEFORE parseBlock / parseItem run, so content
    // keys like {"texture":"ruby_mod:foo"} resolve to live RuntimeIDs.
    scanPackTextures(dir, result.manifest.id, next_texture_id);

    if (!scanDir(dir / "blocks", result, true,  result.error)) return result;
    if (!scanDir(dir / "items",  result, false, result.error)) return result;

    result.hash = computePackHash(dir);
    result.ok   = true;
    return result;
}

// String ↔ enum helpers local to this translation unit — the global
// ContentProfiles already handles tool/category/rarity/behavior; we keep
// MobCategory and Biome parsing here to avoid broadening that surface.
std::optional<MobCategory> mobCategoryFromName(std::string_view s)
{
    if (s == "hostile") return MobCategory::Hostile;
    if (s == "passive") return MobCategory::Passive;
    if (s == "ambient") return MobCategory::Ambient;
    if (s == "water")   return MobCategory::Water;
    return std::nullopt;
}

std::optional<Biome> biomeFromName(std::string_view s)
{
    if (s == "plains")       return Biome::Plains;
    if (s == "forest")       return Biome::Forest;
    if (s == "ocean")        return Biome::Ocean;
    if (s == "desert")       return Biome::Desert;
    if (s == "savanna")      return Biome::Savanna;
    if (s == "mountains")    return Biome::Mountains;
    if (s == "snow")         return Biome::Snow;
    if (s == "jungle")       return Biome::Jungle;
    if (s == "taiga")        return Biome::Taiga;
    if (s == "snowy_taiga")  return Biome::SnowyTaiga;
    if (s == "giant_taiga")  return Biome::GiantTaiga;
    return std::nullopt;
}

std::size_t scanPackSpawnRules(const std::filesystem::path& packDir,
                               const std::string& packNamespace,
                               MobSpawnConfig& out)
{
    const auto dir = packDir / "spawn_rules";
    if (!std::filesystem::exists(dir)) return 0;

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        const std::string raw = readFile(entry.path());
        if (raw.empty()) continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(raw); }
        catch (const std::exception& e)
        {
            std::cerr << "[Pack] spawn_rule parse error " << entry.path()
                      << ": " << e.what() << '\n';
            continue;
        }

        // Namespace scope — the file's own "id" (optional) must match the
        // pack namespace. Having an id is handy for logs but not required.
        if (j.contains("id") && j["id"].is_string() && !packNamespace.empty())
        {
            const std::string prefix = packNamespace + ":";
            const auto id = j["id"].get<std::string>();
            if (id.compare(0, prefix.size(), prefix) != 0)
            {
                std::cerr << "[Pack] spawn_rule " << entry.path()
                          << " id '" << id << "' outside namespace '"
                          << packNamespace << "' — skipped\n";
                continue;
            }
        }

        SpawnEntry se;
        se.prefab   = j.value("prefab", std::string{});
        if (se.prefab.empty())
        {
            std::cerr << "[Pack] spawn_rule " << entry.path() << ": missing 'prefab'\n";
            continue;
        }

        const std::string cat_name = j.value("category", "passive");
        auto cat = mobCategoryFromName(cat_name);
        if (!cat.has_value())
        {
            std::cerr << "[Pack] spawn_rule " << entry.path()
                      << ": unknown category '" << cat_name << "'\n";
            continue;
        }
        se.category = *cat;

        se.weight   = j.value("weight",   1);
        se.min_pack = j.value("min_pack", 1);
        se.max_pack = j.value("max_pack", 1);
        se.day_allowed   = j.value("day_allowed",   true);
        se.night_allowed = j.value("night_allowed", true);
        se.max_block_light = static_cast<uint8_t>(j.value("max_block_light", 15));
        se.max_total_light = static_cast<uint8_t>(j.value("max_total_light", 15));

        if (j.contains("entity_size") && j["entity_size"].is_array() && j["entity_size"].size() == 2)
        {
            se.entity_size = {j["entity_size"][0].get<double>(),
                              j["entity_size"][1].get<double>()};
        }

        if (j.contains("surface") && j["surface"].is_object())
        {
            const auto& s = j["surface"];
            se.surface.required_air_above = s.value("required_air_above", 2);
            se.surface.requires_sky       = s.value("requires_sky",       false);
            se.surface.forbids_sky        = s.value("forbids_sky",        false);
            se.surface.requires_water     = s.value("requires_water",     false);
            if (s.contains("allowed_ground") && s["allowed_ground"].is_array())
            {
                for (const auto& g : s["allowed_ground"])
                    if (g.is_string())
                        se.surface.allowed_ground.push_back(g.get<std::string>());
            }
        }

        if (!j.contains("biomes") || !j["biomes"].is_array())
        {
            std::cerr << "[Pack] spawn_rule " << entry.path() << ": missing 'biomes'\n";
            continue;
        }
        for (const auto& b_name_json : j["biomes"])
        {
            if (!b_name_json.is_string()) continue;
            const auto b = biomeFromName(b_name_json.get<std::string>());
            if (!b.has_value())
            {
                std::cerr << "[Pack] spawn_rule " << entry.path()
                          << ": unknown biome '" << b_name_json.get<std::string>() << "'\n";
                continue;
            }
            out.biome_tables[*b].entries[se.category].push_back(se);
            ++count;
        }
    }
    return count;
}

std::size_t scanPackStructures(const std::filesystem::path& packDir,
                               const std::string& packNamespace)
{
    const auto dir = packDir / "structures";
    if (!std::filesystem::exists(dir)) return 0;

    auto anchorFromName = [](std::string_view s) -> std::optional<StructureRule::Anchor>
    {
        if (s == "surface")       return StructureRule::Anchor::Surface;
        if (s == "fixed")         return StructureRule::Anchor::Fixed;
        if (s == "below_surface") return StructureRule::Anchor::BelowSurface;
        return std::nullopt;
    };

    auto biomeFromString = [](std::string_view s) -> std::optional<Biome>
    {
        if (s == "plains")      return Biome::Plains;
        if (s == "forest")      return Biome::Forest;
        if (s == "ocean")       return Biome::Ocean;
        if (s == "desert")      return Biome::Desert;
        if (s == "savanna")     return Biome::Savanna;
        if (s == "mountains")   return Biome::Mountains;
        if (s == "snow")        return Biome::Snow;
        if (s == "jungle")      return Biome::Jungle;
        if (s == "taiga")       return Biome::Taiga;
        if (s == "snowy_taiga") return Biome::SnowyTaiga;
        if (s == "giant_taiga") return Biome::GiantTaiga;
        return std::nullopt;
    };

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        const std::string raw = readFile(entry.path());
        if (raw.empty()) continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(raw); }
        catch (const std::exception& e)
        {
            std::cerr << "[Pack] structure parse " << entry.path()
                      << ": " << e.what() << '\n';
            continue;
        }

        const std::string key = j.value("id", std::string{});
        if (key.empty())
        {
            std::cerr << "[Pack] structure " << entry.path() << ": missing 'id'\n";
            continue;
        }
        if (!packNamespace.empty())
        {
            const std::string prefix = packNamespace + ":";
            if (key.compare(0, prefix.size(), prefix) != 0)
            {
                std::cerr << "[Pack] structure " << entry.path() << " id '"
                          << key << "' outside namespace '"
                          << packNamespace << "' — skipped\n";
                continue;
            }
        }

        const std::string tmpl_rel = j.value("template", std::string{});
        if (tmpl_rel.empty())
        {
            std::cerr << "[Pack] structure " << entry.path() << ": missing 'template'\n";
            continue;
        }

        // Load the .bbt sibling.
        StructureTemplate tmpl;
        {
            std::ifstream in(dir / tmpl_rel, std::ios::binary);
            if (!in)
            {
                std::cerr << "[Pack] structure " << entry.path()
                          << ": cannot open template " << tmpl_rel << '\n';
                continue;
            }
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                       std::istreambuf_iterator<char>());
            try
            {
                BBT bbt = BBT::load(bytes);
                tmpl.deserialize(bbt.root().at("template"));
            }
            catch (const std::exception& e)
            {
                std::cerr << "[Pack] structure " << entry.path()
                          << ": bbt decode failed: " << e.what() << '\n';
                continue;
            }
        }
        if (tmpl.width == 0 || tmpl.height == 0)
        {
            std::cerr << "[Pack] structure " << entry.path()
                      << ": template " << tmpl_rel << " has zero extent\n";
            continue;
        }

        StructureRule rule;
        rule.key = key;
        if (j.contains("rule") && j["rule"].is_object())
        {
            const auto& r = j["rule"];
            rule.grid            = r.value("grid",            128);
            rule.spawn_chance    = r.value("spawn_chance",    0.0f);
            rule.min_surface_y   = r.value("min_surface_y",   0);
            rule.max_surface_y   = r.value("max_surface_y",   255);
            rule.target_y        = r.value("target_y",        0);
            rule.allow_mirror    = r.value("allow_mirror",    true);
            rule.max_chain_depth = r.value("max_chain_depth", 0);

            const std::string anchor_name = r.value("anchor_mode", "surface");
            if (auto a = anchorFromName(anchor_name); a.has_value())
                rule.anchor_mode = *a;
            else
            {
                std::cerr << "[Pack] structure " << entry.path()
                          << ": unknown anchor_mode '" << anchor_name << "'\n";
                continue;
            }

            if (r.contains("biomes") && r["biomes"].is_array())
            {
                for (const auto& b : r["biomes"])
                {
                    if (!b.is_string()) continue;
                    if (auto bid = biomeFromString(b.get<std::string>()); bid.has_value())
                        rule.biomes.push_back(*bid);
                }
            }
        }

        // Optional template-side overlays. The .bbt file already carries the
        // definitive cell grid (plus any palettes / jigsaws authored in the
        // editor). JSON is allowed to tweak the "allow_mirror_y" bit without
        // requiring an editor round-trip — this is the one knob authors most
        // often want to A/B test on an existing template.
        if (j.contains("allow_mirror_y") && j["allow_mirror_y"].is_boolean())
            tmpl.allow_mirror_y = j["allow_mirror_y"].get<bool>();

        StructureRegistry::add(key, std::move(tmpl), std::move(rule));
        ++count;
    }
    return count;
}

// ----- Loot tables -------------------------------------------------------
//
// <packDir>/loot_tables/*.json. Each file:
//   { "id": "blockbit:mineshaft_chest",
//     "min_rolls": 2, "max_rolls": 5,
//     "entries": [
//       {"item": "blockbit:iron_ingot", "min": 1, "max": 3, "weight": 10},
//       {"item": "blockbit:diamond",    "min": 1, "max": 1, "weight": 1}
//     ] }
//
// The table is registered under "id" and referenced from structure chests via
// `loot` on the block entity marker. Items resolve against gameRegistries().
std::size_t scanPackLootTables(const std::filesystem::path& packDir,
                               const std::string& packNamespace)
{
    const auto dir = packDir / "loot_tables";
    if (!std::filesystem::exists(dir)) return 0;

    const auto& item_reg = gameRegistries().items;

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        const std::string raw = readFile(entry.path());
        if (raw.empty()) continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(raw); }
        catch (const std::exception& e)
        {
            std::cerr << "[Pack] loot parse " << entry.path()
                      << ": " << e.what() << '\n';
            continue;
        }

        const std::string key = j.value("id", std::string{});
        if (key.empty())
        {
            std::cerr << "[Pack] loot " << entry.path() << ": missing 'id'\n";
            continue;
        }
        if (!packNamespace.empty())
        {
            const std::string prefix = packNamespace + ":";
            if (key.compare(0, prefix.size(), prefix) != 0)
            {
                std::cerr << "[Pack] loot " << entry.path() << " id '"
                          << key << "' outside namespace '"
                          << packNamespace << "' — skipped\n";
                continue;
            }
        }

        LootTable table;
        table.min_rolls = static_cast<uint16_t>(j.value("min_rolls", 1));
        table.max_rolls = static_cast<uint16_t>(j.value("max_rolls", 3));

        if (!j.contains("entries") || !j["entries"].is_array())
        {
            std::cerr << "[Pack] loot " << entry.path() << ": missing 'entries' array\n";
            continue;
        }

        bool any_bad = false;
        for (const auto& e : j["entries"])
        {
            if (!e.is_object()) continue;
            const std::string item_key = e.value("item", std::string{});
            auto rid = item_reg.find(item_key);
            if (!rid.has_value())
            {
                std::cerr << "[Pack] loot " << entry.path()
                          << " unresolved item '" << item_key << "' — skipped entry\n";
                any_bad = true;
                continue;
            }
            LootTableEntry le;
            le.item      = item_key;
            le.min_count = static_cast<uint16_t>(e.value("min",    1));
            le.max_count = static_cast<uint16_t>(e.value("max",    1));
            le.weight    = static_cast<uint16_t>(e.value("weight", 1));
            table.entries.push_back(le);
        }

        if (table.entries.empty())
        {
            if (!any_bad)
                std::cerr << "[Pack] loot " << entry.path() << ": no resolvable entries\n";
            continue;
        }

        StructureRegistry::addLootTable(key, std::move(table));
        ++count;
    }
    return count;
}

// ----- Jigsaw pools ------------------------------------------------------
//
// <packDir>/jigsaw_pools/*.json. Each file:
//   { "id": "blockbit:mineshaft_corridors",
//     "entries": [
//       {"structure": "blockbit:mineshaft_corridor", "weight": 3},
//       {"structure": "blockbit:mineshaft_junction", "weight": 1}
//     ] }
//
// Must run AFTER scanPackStructures so structure keys resolve to live IDs.
std::size_t scanPackJigsawPools(const std::filesystem::path& packDir,
                                const std::string& packNamespace)
{
    const auto dir = packDir / "jigsaw_pools";
    if (!std::filesystem::exists(dir)) return 0;

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        const std::string raw = readFile(entry.path());
        if (raw.empty()) continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(raw); }
        catch (const std::exception& e)
        {
            std::cerr << "[Pack] jigsaw_pool parse " << entry.path()
                      << ": " << e.what() << '\n';
            continue;
        }

        const std::string key = j.value("id", std::string{});
        if (key.empty())
        {
            std::cerr << "[Pack] jigsaw_pool " << entry.path() << ": missing 'id'\n";
            continue;
        }
        if (!packNamespace.empty())
        {
            const std::string prefix = packNamespace + ":";
            if (key.compare(0, prefix.size(), prefix) != 0)
            {
                std::cerr << "[Pack] jigsaw_pool " << entry.path() << " id '"
                          << key << "' outside namespace '"
                          << packNamespace << "' — skipped\n";
                continue;
            }
        }

        std::vector<JigsawPoolEntry> entries;
        if (j.contains("entries") && j["entries"].is_array())
        {
            for (const auto& e : j["entries"])
            {
                if (!e.is_object()) continue;
                const std::string skey = e.value("structure", std::string{});
                if (skey.empty()) continue;
                JigsawPoolEntry pe;
                pe.id     = skey;
                pe.weight = static_cast<uint16_t>(e.value("weight", 1));
                entries.push_back(pe);
            }
        }
        if (entries.empty())
        {
            std::cerr << "[Pack] jigsaw_pool " << entry.path()
                      << ": no entries — skipped\n";
            continue;
        }

        StructureRegistry::addJigsawPool(key, std::move(entries));
        ++count;
    }
    return count;
}

std::size_t scanPackRecipes(const std::filesystem::path& packDir,
                            const std::string& packNamespace)
{
    const auto dir = packDir / "recipes";
    if (!std::filesystem::exists(dir)) return 0;

    const auto& item_reg = gameRegistries().items;

    auto resolveItem = [&](const std::string& key,
                           const std::string& context,
                           ItemKey& out) -> bool
    {
        if (auto rid = item_reg.find(key); rid.has_value())
        {
            out = key;
            return true;
        }
        std::cerr << "[Pack] recipe unresolved item '" << key
                  << "' in " << context << " — skipping\n";
        return false;
    };

    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        const std::string raw = readFile(entry.path());
        if (raw.empty()) continue;

        nlohmann::json j;
        try { j = nlohmann::json::parse(raw); }
        catch (const std::exception& e)
        {
            std::cerr << "[Pack] recipe parse " << entry.path()
                      << ": " << e.what() << '\n';
            continue;
        }

        // Namespace scope on "id" (optional but recommended).
        if (j.contains("id") && j["id"].is_string() && !packNamespace.empty())
        {
            const std::string prefix = packNamespace + ":";
            const auto id = j["id"].get<std::string>();
            if (id.compare(0, prefix.size(), prefix) != 0)
            {
                std::cerr << "[Pack] recipe " << entry.path() << " id '"
                          << id << "' outside namespace '"
                          << packNamespace << "' — skipped\n";
                continue;
            }
        }

        const std::string ctx = entry.path().string();
        const std::string type = j.value("type", std::string{});
        if (type == "crafting")
        {
            if (!j.contains("output") || !j["output"].is_object()
                || !j.contains("ingredients") || !j["ingredients"].is_array())
            {
                std::cerr << "[Pack] recipe " << ctx << ": crafting needs output+ingredients\n";
                continue;
            }
            ItemKey out_item{};
            if (!resolveItem(j["output"].value("item", std::string{}), ctx + ".output", out_item))
                continue;
            ItemStack output{out_item, j["output"].value("quantity", 1u)};

            Recipe r;
            r.requires_crafting_table = j.value("requires_crafting_table", false);
            r.ingredients.reserve(j["ingredients"].size());
            bool ok = true;
            for (const auto& ing : j["ingredients"])
            {
                ItemKey iid{};
                if (!resolveItem(ing.value("item", std::string{}), ctx + ".ingredient", iid))
                { ok = false; break; }
                r.ingredients.push_back({iid, ing.value("quantity", 1u)});
            }
            if (!ok) continue;

            recipes[output] = std::move(r);
            ++count;
        }
        else if (type == "smelting")
        {
            ItemKey in_item{}, out_item{};
            if (!resolveItem(j.value("input",  std::string{}), ctx + ".input",  in_item))  continue;
            if (!resolveItem(j.value("output", std::string{}), ctx + ".output", out_item)) continue;
            smeltingRecipes[in_item] = SmeltRecipe{out_item, j.value("cook_time", 10.0f)};
            ++count;
        }
        else if (type == "fuel")
        {
            ItemKey iid{};
            if (!resolveItem(j.value("item", std::string{}), ctx + ".item", iid)) continue;
            fuelBurnTimes[iid] = j.value("burn_time", 1.0f);
            ++count;
        }
        else
        {
            std::cerr << "[Pack] recipe " << ctx
                      << ": unknown type '" << type << "'\n";
        }
    }
    return count;
}

std::size_t scanPackTextures(const std::filesystem::path& packDir,
                             const std::string& packNamespace,
                             RuntimeID& next_texture_id)
{
    const auto tex_dir = packDir / "textures";
    if (!std::filesystem::exists(tex_dir)) return 0;

    std::size_t count = 0;
    std::error_code ec;
    std::vector<std::filesystem::path> texture_files;
    for (auto it = std::filesystem::recursive_directory_iterator(tex_dir, ec);
              it != std::filesystem::recursive_directory_iterator(); ++it)
    {
        if (it->is_regular_file() && it->path().extension() == ".png")
            texture_files.push_back(it->path());
    }
    std::sort(texture_files.begin(), texture_files.end());
    for (const auto& texture_file : texture_files)
    {

        // Texture keys use the pack namespace and the filename only.
        const std::string rel_key = texture_file.stem().string();
        const std::string key     = packNamespace + ":" + rel_key;

        // Registered keys keep their RuntimeID. New keys get a fresh ID;
        // duplicate basenames within a pack deliberately
        // resolve to the same key, with sorted path order deciding the winner.
        RuntimeID id;
        if (auto existing = ContentProfiles::textureFromName(key); existing.has_value())
        {
            id = *existing;
        }
        else
        {
            id = next_texture_id;
            next_texture_id = static_cast<RuntimeID>(
                static_cast<uint32_t>(next_texture_id) + 1);
            ContentProfiles::registerTextureKey(key, id);
        }

        ContentProfiles::registerTextureKey(key, id);

        AssetManager::loadGameTexture(id, texture_file.string());
        ++count;
    }
    if (count > 0)
    {
        std::cout << "[Pack] '" << packNamespace << "' loaded "
                  << count << " texture(s)\n";
    }
    return count;
}

WorldPackCheck validateWorldPacks(const std::filesystem::path& worldDir)
{
    WorldPackCheck r;
    const auto file = worldDir / "packs.json";
    if (!std::filesystem::exists(file)) return r; // predates pack-selection

    std::ifstream in(file);
    if (!in) { r.ok = false; r.reason = "cannot open packs.json"; return r; }

    nlohmann::json j;
    try { in >> j; }
    catch (const std::exception& e) { r.ok = false; r.reason = std::string("packs.json parse: ") + e.what(); return r; }

    if (!j.is_array()) { r.ok = false; r.reason = "packs.json: expected array"; return r; }

    const auto& loaded = gameRegistries().loaded_packs;
    for (const auto& entry : j)
    {
        const auto id   = entry.value("id", std::string{});
        const auto hash = entry.value("hash", static_cast<uint64_t>(0));
        if (id.empty()) continue;

        const LoadedPackSummary* match = nullptr;
        for (const auto& p : loaded) if (p.id == id) { match = &p; break; }

        if (!match)
        {
            r.ok = false;
            r.reason = "world needs pack '" + id + "' which isn't loaded";
            return r;
        }
        if (match->hash != hash)
        {
            r.ok = false;
            r.reason = "pack '" + id + "' hash mismatch (world expects one build, current pack is different)";
            return r;
        }
    }
    return r;
}

} // namespace PackLoader
