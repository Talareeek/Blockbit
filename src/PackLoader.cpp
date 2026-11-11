#include "../include/PackLoader.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../include/Behavior.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/Registry.hpp"

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
            const std::string prefab_name = params.value("projectile", "dynamite");
            if (auto pr = ContentProfiles::prefabFromName(prefab_name); pr.has_value())
                p.projectile = *pr;
            else { err = "unknown prefab '" + prefab_name + "'"; return false; }

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
            p.fluid              = static_cast<BlockID>(params.value("fluid_legacy_id", 0));
            p.source_level_value = static_cast<uint8_t>(params.value("source_level_value", 0));
            p.empty_variant      = static_cast<ItemID>(params.value("empty_variant_legacy_id", 0));
            p.filled_variant     = static_cast<ItemID>(params.value("filled_variant_legacy_id", 0));
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
            const std::string prefab_name = params.value("falling_prefab", "falling_block");
            if (auto pr = ContentProfiles::prefabFromName(prefab_name); pr.has_value())
                p.falling_prefab = *pr;
            else { err = "unknown falling_prefab '" + prefab_name + "'"; return false; }
            out.params = p;
            return true;
        }

        case BlockBehaviorType::PlantGrowth:
        {
            PlantGrowthParams p{};
            p.max_height = static_cast<uint8_t>(params.value("max_height", 1));
            p.grows_as   = static_cast<BlockID>(params.value("grows_as_legacy_id", 0));
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

    out.legacy_id = static_cast<decltype(out.legacy_id)>(j.value("legacy_id", 0));

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
        dp.drop                    = static_cast<ItemID>(d.value("item_legacy_id", 0));
        dp.min_amount              = static_cast<uint8_t>(d.value("min", 1));
        dp.max_amount              = static_cast<uint8_t>(d.value("max", 1));
        dp.alternative_drop_chance = d.value("alternative_chance", 0.0f);
        dp.alternative_drop        = static_cast<ItemID>(d.value("alternative_item_legacy_id", 0));
        b.drop = dp;
    }

    const std::string cb_profile = j.value("callbacks", "none");
    if (!ContentProfiles::applyCallbackProfile(b.callbacks, cb_profile))
    {
        err = file.string() + ": unknown callback profile '" + cb_profile + "'";
        return false;
    }

    // support_blocks_below carries numeric legacy ids for now.
    if (j.contains("support_blocks_below_legacy") && j["support_blocks_below_legacy"].is_array())
    {
        for (const auto& v : j["support_blocks_below_legacy"])
            b.support_blocks_below.push_back(static_cast<BlockID>(v.get<int>()));
    }

    if (j.contains("behavior") && j["behavior"].is_object())
    {
        std::string bh_err;
        if (!parseBlockBehaviorJson(j["behavior"], b.behavior, bh_err))
        {
            err = file.string() + ": behavior: " + bh_err;
            return false;
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

    out.legacy_id = static_cast<decltype(out.legacy_id)>(j.value("legacy_id", 0));

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

LoadResult loadPack(const std::filesystem::path& dir)
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
        result.manifest.id      = j.value("id", std::string{});
        result.manifest.name    = j.value("name", std::string{});
        result.manifest.version = j.value("version", std::string{});
        result.manifest.format  = j.value("format", 1u);
    }
    catch (const std::exception& e)
    {
        result.error = manifest_path.string() + ": " + e.what();
        return result;
    }

    if (!scanDir(dir / "blocks", result, true,  result.error)) return result;
    if (!scanDir(dir / "items",  result, false, result.error)) return result;

    result.hash = computePackHash(dir);
    result.ok   = true;
    return result;
}

LoadAllResult loadAll(const std::vector<std::filesystem::path>& dirs)
{
    LoadAllResult agg;
    agg.packs.reserve(dirs.size());
    for (std::size_t i = 0; i < dirs.size(); ++i)
    {
        LoadResult r = loadPack(dirs[i]);
        if (!r.ok)
        {
            agg.ok            = false;
            agg.error         = r.error;
            agg.failing_index = i;
            agg.packs.push_back(std::move(r));
            return agg;
        }
        agg.packs.push_back(std::move(r));
    }
    return agg;
}

void applyToDatabases(const LoadResult& r)
{
    std::size_t blocks = 0, items = 0;
    for (const auto& lb : r.blocks)
    {
        blockDatabase[lb.legacy_id] = lb.data;
        ++blocks;
    }
    for (const auto& li : r.items)
    {
        itemDatabase[li.legacy_id] = li.data;
        ++items;
    }
    std::cout << "[Pack] Applied " << blocks << " blocks and "
              << items << " items to live databases\n";
}

} // namespace PackLoader
