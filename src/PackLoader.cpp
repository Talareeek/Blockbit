#include "../include/PackLoader.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../include/ContentProfiles.hpp"
#include "../include/Registry.hpp"

namespace PackLoader
{

namespace
{

using json = nlohmann::json;

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

    // behavior is accepted but not expanded — Phase 1c will wire the variant
    // params via an explicit parser once we commit to full migration.

    return true;
}

bool scanDir(const std::filesystem::path& dir,
             LoadResult& result,
             bool is_block,
             std::string& err)
{
    if (!std::filesystem::exists(dir)) return true;

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        if (is_block)
        {
            LoadedBlock lb;
            if (!parseBlock(entry.path(), lb, err)) return false;
            result.blocks.push_back(std::move(lb));
        }
        else
        {
            LoadedItem li;
            if (!parseItem(entry.path(), li, err)) return false;
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

    result.ok = true;
    return result;
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
        // Preserve the existing onUse lambda if the pack entry carries no
        // behavior — otherwise items ported in Phase 1b whose JSON doesn't
        // yet encode the variant params would silently lose their effect.
        auto existing = itemDatabase.find(li.legacy_id);
        ItemData merged = li.data;
        if (existing != itemDatabase.end())
        {
            merged.onUse    = existing->second.onUse;
            if (merged.behavior.type == ItemBehaviorType::None)
                merged.behavior = existing->second.behavior;
        }
        itemDatabase[li.legacy_id] = merged;
        ++items;
    }
    std::cout << "[Pack] Applied " << blocks << " blocks and "
              << items << " items to live databases\n";
}

} // namespace PackLoader
