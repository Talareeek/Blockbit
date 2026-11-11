#include "../include/ContentProfiles.hpp"

#include <array>
#include <unordered_map>

namespace
{

// Shared bidirectional map built from a flat list. The on-disk string is
// what's baked into JSON; changing a column here is a format break.
template <typename Enum>
struct BiMap
{
    std::vector<std::pair<Enum, std::string>> entries;

    std::optional<Enum> fromName(std::string_view name) const
    {
        for (const auto& [e, s] : entries)
            if (s == name) return e;
        return std::nullopt;
    }
    std::string toName(Enum value) const
    {
        for (const auto& [e, s] : entries)
            if (e == value) return s;
        return "";
    }
};

using TextureMap = BiMap<AssetManager::GameTextureID>;

const TextureMap& textureMap()
{
    using TID = AssetManager::GameTextureID;
    static const TextureMap m{{
        {TID::Stone,                 "stone"},
        {TID::Grass,                 "grass"},
        {TID::Dirt,                  "dirt"},
        {TID::Cobblestone,           "cobblestone"},
        {TID::Obsidian,              "obsidian"},
        {TID::Bedrock,               "bedrock"},
        {TID::Water,                 "water"},
        {TID::Lava,                  "lava"},
        {TID::Coal_Ore,              "coal_ore"},
        {TID::Iron_Ore,              "iron_ore"},
        {TID::Gold_Ore,              "gold_ore"},
        {TID::Diamond_Ore,           "diamond_ore"},
        {TID::Ruby_Ore,              "ruby_ore"},
        {TID::Iron_Block,            "iron_block"},
        {TID::Gold_Block,            "gold_block"},
        {TID::Ruby_Block,            "ruby_block"},
        {TID::Coal_Block,            "coal_block"},
        {TID::Oak_Log,               "oak_log"},
        {TID::Oak_Leaves,            "oak_leaves"},
        {TID::Oak_Planks,            "oak_planks"},
        {TID::Birch_Log,             "birch_log"},
        {TID::Birch_Leaves,          "birch_leaves"},
        {TID::Spruce_Log,            "spruce_log"},
        {TID::Spruce_Leaves,         "spruce_leaves"},
        {TID::Woodcutter,            "woodcutter"},
        {TID::Fire,                  "fire"},
        {TID::Sand,                  "sand"},
        {TID::Coarse_Dirt,           "coarse_dirt"},
        {TID::Snow,                  "snow"},
        {TID::Chest,                 "chest"},
        {TID::Bricks,                "bricks"},
        {TID::Furnace,               "furnace"},
        {TID::Workbench,             "workbench"},
        {TID::Gravel,                "gravel"},
        {TID::Cactus,                "cactus"},
        {TID::Dead_Bush,             "dead_bush"},
        {TID::Smooth_Stone,          "smooth_stone"},
        {TID::Diorite,               "diorite"},
        {TID::Granite,               "granite"},
        {TID::Glass,                 "glass"},
        {TID::Output_Bulb,           "output_bulb"},
        {TID::Ruby_Wire,             "ruby_wire"},
        {TID::Coreheart_Of_Blockbit, "coreheart_of_blockbit"},
        {TID::Upgrade_Table,         "upgrade_table"},
        {TID::Dynamite,              "dynamite"},
        {TID::Bucket,                "bucket"},
        {TID::Water_Bucket,          "water_bucket"},
        {TID::Lighter,               "lighter"},
        {TID::Bottle,                "bottle"},
        {TID::Coal,                  "coal"},
        {TID::Raw_Iron,              "raw_iron"},
        {TID::Raw_Gold,              "raw_gold"},
        {TID::Iron_Ingot,            "iron_ingot"},
        {TID::Gold_Ingot,            "gold_ingot"},
        {TID::Diamond,               "diamond"},
        {TID::Ruby,                  "ruby"},
        {TID::Stick,                 "stick"},
        {TID::Flint,                 "flint"},
        {TID::Player,                "player"},
        {TID::Pig,                   "pig"},
        {TID::Cube,                  "cube"},
        {TID::Wooden_Pickaxe,        "wooden_pickaxe"},
        {TID::Stone_Pickaxe,         "stone_pickaxe"},
        {TID::Iron_Pickaxe,          "iron_pickaxe"},
        {TID::Gold_Pickaxe,          "gold_pickaxe"},
        {TID::Diamond_Pickaxe,       "diamond_pickaxe"},
        {TID::Ruby_Pickaxe,          "ruby_pickaxe"},
        {TID::Wooden_Axe,            "wooden_axe"},
        {TID::Stone_Axe,             "stone_axe"},
        {TID::Iron_Axe,              "iron_axe"},
        {TID::Gold_Axe,              "gold_axe"},
        {TID::Diamond_Axe,           "diamond_axe"},
        {TID::Ruby_Axe,              "ruby_axe"},
        {TID::Wooden_Shovel,         "wooden_shovel"},
        {TID::Stone_Shovel,          "stone_shovel"},
        {TID::Iron_Shovel,           "iron_shovel"},
        {TID::Gold_Shovel,           "gold_shovel"},
        {TID::Diamond_Shovel,        "diamond_shovel"},
        {TID::Ruby_Shovel,           "ruby_shovel"},
    }};
    return m;
}

} // namespace

// Forward decls for the five vanilla callback sets. Defined inside the
// blockbit_callbacks namespace in Block.cpp (named, not anonymous) so packs
// can wire them in by profile name. Must live at global scope so the
// namespace resolves to ::blockbit_callbacks, not ContentProfiles::.
namespace blockbit_callbacks
{
void falling_on_place(BlockUpdateContext&, sf::Vector2i);
void falling_on_post_placement(BlockUpdateContext&, sf::Vector2i, uint8_t);
void falling_on_scheduled_tick(BlockUpdateContext&, sf::Vector2i);
void water_on_place(BlockUpdateContext&, sf::Vector2i);
void water_on_neighbor_changed(BlockUpdateContext&, sf::Vector2i, sf::Vector2i);
void water_on_scheduled_tick(BlockUpdateContext&, sf::Vector2i);
void lava_on_place(BlockUpdateContext&, sf::Vector2i);
void lava_on_neighbor_changed(BlockUpdateContext&, sf::Vector2i, sf::Vector2i);
void lava_on_scheduled_tick(BlockUpdateContext&, sf::Vector2i);
void cactus_on_post_placement(BlockUpdateContext&, sf::Vector2i, uint8_t);
void cactus_on_random_tick(BlockUpdateContext&, sf::Vector2i);
}

namespace ContentProfiles
{

using namespace blockbit_callbacks;

// Runtime key→id map. Seeded on first use with the vanilla name table
// (vanilla "<name>" entries become "blockbit:<name>" keys); modpack texture
// loads add further entries past the vanilla enum range.
std::unordered_map<std::string, AssetManager::GameTextureID>& textureKeyMap()
{
    static std::unordered_map<std::string, AssetManager::GameTextureID> map = [] {
        std::unordered_map<std::string, AssetManager::GameTextureID> m;
        for (const auto& [id, name] : textureMap().entries)
            m.emplace("blockbit:" + std::string(name), id);
        return m;
    }();
    return map;
}

std::optional<AssetManager::GameTextureID> textureFromName(std::string_view name)
{
    if (name.find(':') != std::string_view::npos)
    {
        const std::string key(name);
        auto it = textureKeyMap().find(key);
        if (it != textureKeyMap().end()) return it->second;
        return std::nullopt;
    }
    return textureMap().fromName(name);
}

std::string nameFromTexture(AssetManager::GameTextureID id)
{
    return textureMap().toName(id);
}

void registerTextureKey(const std::string& key, AssetManager::GameTextureID id)
{
    textureKeyMap()[key] = id;
}

AssetManager::GameTextureID firstDynamicTextureId()
{
    return static_cast<AssetManager::GameTextureID>(1000);
}

// --- Prefab ---------------------------------------------------------------

std::optional<PrefabID> prefabFromName(std::string_view name)
{
    if (name == "player")        return PrefabID::Player;
    if (name == "pig")           return PrefabID::Pig;
    if (name == "dynamite")      return PrefabID::Dynamite;
    if (name == "dropped_item")  return PrefabID::DroppedItem;
    if (name == "falling_block") return PrefabID::FallingBlock;
    if (name == "cube")          return PrefabID::Cube;
    return std::nullopt;
}

std::string nameFromPrefab(PrefabID id)
{
    switch (id)
    {
        case PrefabID::Player:        return "player";
        case PrefabID::Pig:           return "pig";
        case PrefabID::Dynamite:      return "dynamite";
        case PrefabID::DroppedItem:   return "dropped_item";
        case PrefabID::FallingBlock:  return "falling_block";
        case PrefabID::Cube:          return "cube";
    }
    return "";
}

// --- Tool / category / rarity --------------------------------------------

std::optional<ToolType> toolTypeFromName(std::string_view name)
{
    if (name == "pickaxe") return ToolType::Pickaxe;
    if (name == "axe")     return ToolType::Axe;
    if (name == "shovel")  return ToolType::Shovel;
    if (name == "none")    return ToolType::None;
    return std::nullopt;
}

std::string nameFromToolType(ToolType t)
{
    switch (t)
    {
        case ToolType::Pickaxe: return "pickaxe";
        case ToolType::Axe:     return "axe";
        case ToolType::Shovel:  return "shovel";
        case ToolType::None:    return "none";
    }
    return "none";
}

std::optional<ItemCategory> categoryFromName(std::string_view name)
{
    if (name == "block")      return ItemCategory::Block;
    if (name == "tool")       return ItemCategory::Tool;
    if (name == "weapon")     return ItemCategory::Weapon;
    if (name == "consumable") return ItemCategory::Consumable;
    if (name == "misc")       return ItemCategory::Misc;
    return std::nullopt;
}

std::string nameFromCategory(ItemCategory c)
{
    switch (c)
    {
        case ItemCategory::Block:      return "block";
        case ItemCategory::Tool:       return "tool";
        case ItemCategory::Weapon:     return "weapon";
        case ItemCategory::Consumable: return "consumable";
        case ItemCategory::Misc:       return "misc";
    }
    return "misc";
}

std::optional<ItemRarity> rarityFromName(std::string_view name)
{
    if (name == "common")     return ItemRarity::Common;
    if (name == "rare")       return ItemRarity::Rare;
    if (name == "super_rare") return ItemRarity::Super_Rare;
    if (name == "epic")       return ItemRarity::Epic;
    if (name == "mythic")     return ItemRarity::Mythic;
    return std::nullopt;
}

std::string nameFromRarity(ItemRarity r)
{
    switch (r)
    {
        case ItemRarity::Common:     return "common";
        case ItemRarity::Rare:       return "rare";
        case ItemRarity::Super_Rare: return "super_rare";
        case ItemRarity::Epic:       return "epic";
        case ItemRarity::Mythic:     return "mythic";
    }
    return "common";
}

// --- Behavior types ------------------------------------------------------

std::optional<ItemBehaviorType> itemBehaviorTypeFromName(std::string_view name)
{
    if (name == "none")             return ItemBehaviorType::None;
    if (name == "throw_projectile") return ItemBehaviorType::ThrowProjectile;
    if (name == "fluid_bucket")     return ItemBehaviorType::FluidBucket;
    if (name == "ignite")           return ItemBehaviorType::Ignite;
    if (name == "consume_effect")   return ItemBehaviorType::ConsumeEffect;
    return std::nullopt;
}

std::string nameFromItemBehaviorType(ItemBehaviorType t)
{
    switch (t)
    {
        case ItemBehaviorType::None:            return "none";
        case ItemBehaviorType::ThrowProjectile: return "throw_projectile";
        case ItemBehaviorType::FluidBucket:     return "fluid_bucket";
        case ItemBehaviorType::Ignite:          return "ignite";
        case ItemBehaviorType::ConsumeEffect:   return "consume_effect";
    }
    return "none";
}

std::optional<BlockBehaviorType> blockBehaviorTypeFromName(std::string_view name)
{
    if (name == "none")          return BlockBehaviorType::None;
    if (name == "cascade_fluid") return BlockBehaviorType::CascadeFluid;
    if (name == "gravity_block") return BlockBehaviorType::GravityBlock;
    if (name == "plant_growth")  return BlockBehaviorType::PlantGrowth;
    return std::nullopt;
}

std::string nameFromBlockBehaviorType(BlockBehaviorType t)
{
    switch (t)
    {
        case BlockBehaviorType::None:         return "none";
        case BlockBehaviorType::CascadeFluid: return "cascade_fluid";
        case BlockBehaviorType::GravityBlock: return "gravity_block";
        case BlockBehaviorType::PlantGrowth:  return "plant_growth";
    }
    return "none";
}

std::optional<ThrowProjectileParams::Pattern> throwPatternFromName(std::string_view name)
{
    if (name == "single") return ThrowProjectileParams::Pattern::Single;
    if (name == "column") return ThrowProjectileParams::Pattern::Column;
    if (name == "arc")    return ThrowProjectileParams::Pattern::Arc;
    return std::nullopt;
}

std::string nameFromThrowPattern(ThrowProjectileParams::Pattern p)
{
    switch (p)
    {
        case ThrowProjectileParams::Pattern::Single: return "single";
        case ThrowProjectileParams::Pattern::Column: return "column";
        case ThrowProjectileParams::Pattern::Arc:    return "arc";
    }
    return "single";
}

// --- Render profile ------------------------------------------------------

bool applyRenderProfile(RenderProperties& out, std::string_view profile)
{
    if (profile == "default")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = default_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "water")
    {
        out.render_bounds = water_render_bounds;
        out.rect          = water_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "lava")
    {
        out.render_bounds = lava_render_bounds;
        out.rect          = lava_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "chest")
    {
        out.render_bounds = chest_render_bounds;
        out.rect          = chest_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "furnace")
    {
        out.render_bounds = furnace_render_bounds;
        out.rect          = furnace_render_rect;
        out.animation     = furnace_animation;
        return true;
    }
    if (profile == "cactus")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = cactus_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "woodcutter")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = woodcutter_render_rect;
        out.animation     = woodcutter_animation;
        return true;
    }
    if (profile == "connected")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = connected_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "coreheart")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = coreheart_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "output_bulb")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = output_bulb_render_rect;
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    return false;
}

std::string renderProfileOf(const RenderProperties& /*props*/)
{
    // std::function has no comparable identity — the exporter uses an
    // explicit BlockID → profile table instead. Returning "default" here
    // is a safe placeholder that loaders would accept.
    return "default";
}

// --- Callback profile ----------------------------------------------------

bool applyCallbackProfile(BlockUpdateCallbacks& out, std::string_view profile)
{
    out = BlockUpdateCallbacks{};
    if (profile == "none" || profile.empty()) return true;
    if (profile == "water")
    {
        out.onPlace            = water_on_place;
        out.onNeighborChanged  = water_on_neighbor_changed;
        out.onScheduledTick    = water_on_scheduled_tick;
        return true;
    }
    if (profile == "lava")
    {
        out.onPlace            = lava_on_place;
        out.onNeighborChanged  = lava_on_neighbor_changed;
        out.onScheduledTick    = lava_on_scheduled_tick;
        return true;
    }
    if (profile == "falling")
    {
        out.onPlace            = falling_on_place;
        out.onPostPlacement    = falling_on_post_placement;
        out.onScheduledTick    = falling_on_scheduled_tick;
        return true;
    }
    if (profile == "cactus")
    {
        out.onPostPlacement    = cactus_on_post_placement;
        out.onRandomTick       = cactus_on_random_tick;
        return true;
    }
    return false;
}

std::string callbackProfileOf(const BlockUpdateCallbacks& cb)
{
    // Exporter path — identifies the profile by function-pointer identity.
    if (cb.onPlace == water_on_place)      return "water";
    if (cb.onPlace == lava_on_place)       return "lava";
    if (cb.onPlace == falling_on_place)    return "falling";
    if (cb.onPostPlacement == cactus_on_post_placement) return "cactus";
    return "none";
}

} // namespace ContentProfiles
