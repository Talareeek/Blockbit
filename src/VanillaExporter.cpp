#include "../include/VanillaExporter.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "../include/Block.hpp"
#include "../include/Behavior.hpp"
#include "../include/ContentProfiles.hpp"
#include "../include/Item.hpp"

namespace VanillaExporter
{

namespace
{

using json = nlohmann::json;

// Hand-coded render profile map for vanilla blocks. Any id not listed falls
// back to "default" — that's intentional: most vanilla blocks use the same
// render pair. Mirrors blockDatabase initializers in src/Block.cpp.
const std::unordered_map<BlockID, std::string>& renderProfileMap()
{
    static const std::unordered_map<BlockID, std::string> m = {
        {BlockID::Water,                 "water"},
        {BlockID::Lava,                  "lava"},
        {BlockID::Woodcutter,            "woodcutter"},
        {BlockID::Chest,                 "chest"},
        {BlockID::Furnace,               "furnace"},
        {BlockID::Cactus,                "cactus"},
        {BlockID::Smooth_Stone,          "connected"},
        {BlockID::Ruby_Wire,             "connected"},
        {BlockID::Output_Bulb,           "output_bulb"},
        {BlockID::Coreheart_Of_Blockbit, "coreheart"},
    };
    return m;
}

// Mirrors the block/item name tables in src/GameRegistries.cpp but exposed
// here as a direct index lookup. Keeping two tables in sync is the kind of
// duplication that will bite later — Phase 1c-E should unify them.
constexpr std::string_view kBlockNames[] = {
    "air", "stone", "grass", "dirt", "cobblestone", "obsidian", "bedrock",
    "water", "lava", "coal_ore", "iron_ore", "gold_ore", "diamond_ore",
    "ruby_ore", "iron_block", "gold_block", "ruby_block", "coal_block",
    "oak_log", "oak_leaves", "oak_planks", "birch_log", "birch_leaves",
    "spruce_log", "spruce_leaves", "woodcutter", "fire", "sand",
    "coarse_dirt", "snow", "chest", "bricks", "furnace", "workbench",
    "gravel", "cactus", "dead_bush", "smooth_stone", "diorite", "granite",
    "glass", "output_bulb", "ruby_wire", "coreheart_of_blockbit",
    "upgrade_table"
};

constexpr std::string_view kItemNames[] = {
    "air", "stone", "grass", "dirt", "cobblestone", "obsidian", "bedrock",
    "dynamite", "stab_shot", "nuke_shot", "coal_ore", "iron_ore", "gold_ore",
    "diamond_ore", "ruby_ore", "iron_block", "gold_block", "ruby_block",
    "coal_block", "coal", "raw_iron", "raw_gold", "diamond", "oak_log",
    "oak_leaves", "oak_planks", "birch_log", "birch_leaves", "spruce_log",
    "spruce_leaves", "bucket", "water_bucket", "woodcutter", "lighter",
    "sand", "coarse_dirt", "snow", "gravel", "flint", "wooden_pickaxe",
    "stone_pickaxe", "iron_pickaxe", "gold_pickaxe", "diamond_pickaxe",
    "ruby_pickaxe", "wooden_axe", "stone_axe", "iron_axe", "gold_axe",
    "diamond_axe", "ruby_axe", "wooden_shovel", "stone_shovel", "iron_shovel",
    "gold_shovel", "diamond_shovel", "ruby_shovel", "ruby", "stick", "chest",
    "bricks", "furnace", "iron_ingot", "gold_ingot", "workbench", "cactus",
    "dead_bush", "smooth_stone", "diorite", "granite", "glass", "output_bulb",
    "ruby_wire", "coreheart_of_blockbit", "upgrade_table", "magic_bottle"
};

std::string renderProfileOf(BlockID id)
{
    const auto& m = renderProfileMap();
    if (auto it = m.find(id); it != m.end()) return it->second;
    return "default";
}

json serializeBlockBehavior(const BlockBehavior& bh)
{
    json j;
    j["type"] = ContentProfiles::nameFromBlockBehaviorType(bh.type);
    switch (bh.type)
    {
        case BlockBehaviorType::CascadeFluid:
            if (const auto* p = std::get_if<CascadeFluidParams>(&bh.params))
                j["params"] = {
                    {"max_level",        p->max_level},
                    {"tick_delay_ticks", p->tick_delay_ticks}};
            break;
        case BlockBehaviorType::GravityBlock:
            if (const auto* p = std::get_if<GravityBlockParams>(&bh.params))
                j["params"] = {
                    {"fall_delay_ticks", p->fall_delay_ticks},
                    {"falling_prefab",   ContentProfiles::nameFromPrefab(p->falling_prefab)}};
            break;
        case BlockBehaviorType::PlantGrowth:
            if (const auto* p = std::get_if<PlantGrowthParams>(&bh.params))
                j["params"] = {
                    {"max_height",     p->max_height},
                    {"grows_as_legacy_id", static_cast<int>(p->grows_as)}};
            break;
        case BlockBehaviorType::None:
            break;
    }
    return j;
}

json serializeItemBehavior(const ItemBehavior& bh)
{
    json j;
    j["type"] = ContentProfiles::nameFromItemBehaviorType(bh.type);
    switch (bh.type)
    {
        case ItemBehaviorType::ThrowProjectile:
            if (const auto* p = std::get_if<ThrowProjectileParams>(&bh.params))
                j["params"] = {
                    {"projectile",    ContentProfiles::nameFromPrefab(p->projectile)},
                    {"pattern",       ContentProfiles::nameFromThrowPattern(p->pattern)},
                    {"count",         p->count},
                    {"throw_speed",   p->throw_speed},
                    {"spread",        p->spread},
                    {"height_offset", p->height_offset}};
            break;
        case ItemBehaviorType::FluidBucket:
            if (const auto* p = std::get_if<FluidBucketParams>(&bh.params))
                j["params"] = {
                    {"fluid_legacy_id",         static_cast<int>(p->fluid)},
                    {"source_level_value",      p->source_level_value},
                    {"empty_variant_legacy_id", static_cast<int>(p->empty_variant)},
                    {"filled_variant_legacy_id", static_cast<int>(p->filled_variant)}};
            break;
        case ItemBehaviorType::Ignite:
            if (const auto* p = std::get_if<IgniteParams>(&bh.params))
                j["params"] = {
                    {"consume_on_success", p->consume_on_success},
                    {"can_place_fire",     p->can_place_fire}};
            break;
        case ItemBehaviorType::ConsumeEffect:
            j["params"] = json::object();
            break;
        case ItemBehaviorType::None:
            break;
    }
    return j;
}

json serializeBlock(BlockID id, std::string_view name, const BlockData& b)
{
    json j;
    j["id"]        = std::string("blockbit:") + std::string(name);
    j["legacy_id"] = static_cast<int>(id);

    j["solid"]            = b.solid;
    j["transparent"]      = b.transparent;
    j["breakable"]        = b.breakable;
    j["liquid"]           = b.liquid;
    j["drag"]             = b.drag;
    j["hardness"]         = b.hardness;
    j["light_emission"]   = b.light_emission;
    j["light_opacity"]    = b.light_opacity;
    j["lets_light"]       = b.lets_light;
    j["random_tickable"]  = b.random_tickable;
    j["pp_accept_mask"]   = b.pp_accept_mask;

    if (b.render.has_value())
    {
        j["render"] = {
            {"texture", ContentProfiles::nameFromTexture(b.render->texture)},
            {"profile", renderProfileOf(id)}};
    }
    else
    {
        j["render"] = nullptr;
    }

    if (b.mining.has_value())
    {
        j["mining"] = {
            {"resistance", b.mining->mining_resistance},
            {"tool",       ContentProfiles::nameFromToolType(b.mining->desired_tool.tool_type)},
            {"level",      b.mining->desired_tool.level}};
    }

    if (b.drop.has_value())
    {
        j["drop"] = {
            {"always",                     b.drop->always_drop},
            {"item_legacy_id",             static_cast<int>(b.drop->drop)},
            {"min",                        b.drop->min_amount},
            {"max",                        b.drop->max_amount},
            {"alternative_chance",         b.drop->alternative_drop_chance},
            {"alternative_item_legacy_id", static_cast<int>(b.drop->alternative_drop)}};
    }

    j["callbacks"] = ContentProfiles::callbackProfileOf(b.callbacks);

    if (!b.support_blocks_below.empty())
    {
        json arr = json::array();
        for (auto sid : b.support_blocks_below) arr.push_back(static_cast<int>(sid));
        j["support_blocks_below_legacy"] = arr;
    }

    if (b.behavior.type != BlockBehaviorType::None)
        j["behavior"] = serializeBlockBehavior(b.behavior);

    return j;
}

json serializeItem(ItemID id, std::string_view name, const ItemData& it)
{
    json j;
    j["id"]        = std::string("blockbit:") + std::string(name);
    j["legacy_id"] = static_cast<int>(id);

    j["name"]           = it.name;
    j["texture"]        = ContentProfiles::nameFromTexture(it.texture);
    j["max_stack_size"] = it.maxStackSize;
    j["rarity"]         = ContentProfiles::nameFromRarity(it.rarity);
    j["category"]       = ContentProfiles::nameFromCategory(it.category);
    j["attack_delay"]   = it.attack_delay;
    j["is_bottle"]      = it.is_bottle;

    if (it.tool.has_value())
    {
        j["tool"] = {
            {"type",  ContentProfiles::nameFromToolType(it.tool->tool_type)},
            {"level", it.tool->level}};
    }
    if (it.maxDurability.has_value())
        j["max_durability"] = *it.maxDurability;
    if (it.damage.has_value())
        j["damage"] = *it.damage;

    if (it.behavior.type != ItemBehaviorType::None)
        j["behavior"] = serializeItemBehavior(it.behavior);

    return j;
}

void writeJson(const std::filesystem::path& file, const json& j)
{
    std::ofstream f(file);
    if (!f)
    {
        std::cerr << "VanillaExporter: cannot open " << file << '\n';
        std::exit(1);
    }
    f << j.dump(4) << '\n';
}

} // namespace

ExportStats exportTo(const std::filesystem::path& dir)
{
    ExportStats stats;

    std::filesystem::create_directories(dir / "blocks");
    std::filesystem::create_directories(dir / "items");

    // pack.json
    {
        json manifest = {
            {"id",      "blockbit"},
            {"name",    "Blockbit"},
            {"version", "0.1.0"},
            {"format",  1}
        };
        writeJson(dir / "pack.json", manifest);
    }

    // blocks/*.json
    for (std::size_t i = 0; i < sizeof(kBlockNames)/sizeof(kBlockNames[0]); ++i)
    {
        const auto id = static_cast<BlockID>(i);
        auto it = blockDatabase.find(id);
        if (it == blockDatabase.end()) continue;

        json j = serializeBlock(id, kBlockNames[i], it->second);
        writeJson(dir / "blocks" / (std::string(kBlockNames[i]) + ".json"), j);
        ++stats.blocks_written;
    }

    // items/*.json
    for (std::size_t i = 0; i < sizeof(kItemNames)/sizeof(kItemNames[0]); ++i)
    {
        const auto id = static_cast<ItemID>(i);
        auto it = itemDatabase.find(id);
        if (it == itemDatabase.end()) continue;

        json j = serializeItem(id, kItemNames[i], it->second);
        writeJson(dir / "items" / (std::string(kItemNames[i]) + ".json"), j);
        ++stats.items_written;
    }

    return stats;
}

} // namespace VanillaExporter
