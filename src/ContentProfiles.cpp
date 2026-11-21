#include "../include/ContentProfiles.hpp"
#include "../include/GameRegistries.hpp"

#include <array>

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

using TextureMap = BiMap<RuntimeID>;

const TextureMap& textureMap()
{
    static const TextureMap m{{
        {static_cast<RuntimeID>(0),                 "stone"},
        {static_cast<RuntimeID>(1),                 "grass"},
        {static_cast<RuntimeID>(2),                  "dirt"},
        {static_cast<RuntimeID>(3),           "cobblestone"},
        {static_cast<RuntimeID>(4),              "obsidian"},
        {static_cast<RuntimeID>(5),               "bedrock"},
        {static_cast<RuntimeID>(6),                 "water"},
        {static_cast<RuntimeID>(7),                  "lava"},
        {static_cast<RuntimeID>(8),              "coal_ore"},
        {static_cast<RuntimeID>(9),              "iron_ore"},
        {static_cast<RuntimeID>(10),              "gold_ore"},
        {static_cast<RuntimeID>(11),           "diamond_ore"},
        {static_cast<RuntimeID>(12),              "ruby_ore"},
        {static_cast<RuntimeID>(13),            "iron_block"},
        {static_cast<RuntimeID>(14),            "gold_block"},
        {static_cast<RuntimeID>(15),            "ruby_block"},
        {static_cast<RuntimeID>(16),            "coal_block"},
        {static_cast<RuntimeID>(17),               "oak_log"},
        {static_cast<RuntimeID>(18),            "oak_leaves"},
        {static_cast<RuntimeID>(19),            "oak_planks"},
        {static_cast<RuntimeID>(20),             "birch_log"},
        {static_cast<RuntimeID>(21),          "birch_leaves"},
        {static_cast<RuntimeID>(22),            "spruce_log"},
        {static_cast<RuntimeID>(23),         "spruce_leaves"},
        {static_cast<RuntimeID>(24),            "woodcutter"},
        {static_cast<RuntimeID>(25),                  "fire"},
        {static_cast<RuntimeID>(26),                  "sand"},
        {static_cast<RuntimeID>(27),           "coarse_dirt"},
        {static_cast<RuntimeID>(28),                  "snow"},
        {static_cast<RuntimeID>(29),                 "chest"},
        {static_cast<RuntimeID>(30),                "bricks"},
        {static_cast<RuntimeID>(31),               "furnace"},
        {static_cast<RuntimeID>(32),             "workbench"},
        {static_cast<RuntimeID>(33),                "gravel"},
        {static_cast<RuntimeID>(34),                "cactus"},
        {static_cast<RuntimeID>(35),             "dead_bush"},
        {static_cast<RuntimeID>(36),          "smooth_stone"},
        {static_cast<RuntimeID>(37),               "diorite"},
        {static_cast<RuntimeID>(38),               "granite"},
        {static_cast<RuntimeID>(39),                 "glass"},
        {static_cast<RuntimeID>(40),           "output_bulb"},
        {static_cast<RuntimeID>(41),             "ruby_wire"},
        {static_cast<RuntimeID>(42), "coreheart_of_blockbit"},
        {static_cast<RuntimeID>(43),         "upgrade_table"},
        {static_cast<RuntimeID>(44),          "birch_planks"},
        {static_cast<RuntimeID>(45),         "spruce_planks"},
        {static_cast<RuntimeID>(46),            "copper_ore"},
        {static_cast<RuntimeID>(47),          "copper_ingot"},
        {static_cast<RuntimeID>(48),          "copper_block"},
        {static_cast<RuntimeID>(49),              "dynamite"},
        {static_cast<RuntimeID>(50),                "bucket"},
        {static_cast<RuntimeID>(51),          "water_bucket"},
        {static_cast<RuntimeID>(52),               "lighter"},
        {static_cast<RuntimeID>(53),                "bottle"},
        {static_cast<RuntimeID>(54),                  "coal"},
        {static_cast<RuntimeID>(55),              "raw_iron"},
        {static_cast<RuntimeID>(56),              "raw_gold"},
        {static_cast<RuntimeID>(57),            "iron_ingot"},
        {static_cast<RuntimeID>(58),            "gold_ingot"},
        {static_cast<RuntimeID>(59),               "diamond"},
        {static_cast<RuntimeID>(60),                  "ruby"},
        {static_cast<RuntimeID>(61),                 "stick"},
        {static_cast<RuntimeID>(62),                 "flint"},
        {static_cast<RuntimeID>(63),                "player"},
        {static_cast<RuntimeID>(64),                   "pig"},
        {static_cast<RuntimeID>(65),                   "cow"},
        {static_cast<RuntimeID>(66),                  "cube"},
        {static_cast<RuntimeID>(67),        "wooden_pickaxe"},
        {static_cast<RuntimeID>(68),         "stone_pickaxe"},
        {static_cast<RuntimeID>(69),          "iron_pickaxe"},
        {static_cast<RuntimeID>(70),          "gold_pickaxe"},
        {static_cast<RuntimeID>(71),       "diamond_pickaxe"},
        {static_cast<RuntimeID>(72),          "ruby_pickaxe"},
        {static_cast<RuntimeID>(73),            "wooden_axe"},
        {static_cast<RuntimeID>(74),             "stone_axe"},
        {static_cast<RuntimeID>(75),              "iron_axe"},
        {static_cast<RuntimeID>(76),              "gold_axe"},
        {static_cast<RuntimeID>(77),           "diamond_axe"},
        {static_cast<RuntimeID>(78),              "ruby_axe"},
        {static_cast<RuntimeID>(79),         "wooden_shovel"},
        {static_cast<RuntimeID>(80),          "stone_shovel"},
        {static_cast<RuntimeID>(81),           "iron_shovel"},
        {static_cast<RuntimeID>(82),           "gold_shovel"},
        {static_cast<RuntimeID>(83),        "diamond_shovel"},
        {static_cast<RuntimeID>(84),           "ruby_shovel"},
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

std::optional<RuntimeID> textureFromName(std::string_view name)
{
    if (name.find(':') != std::string_view::npos)
    {
        if (auto id = gameRegistries().textures.find(std::string(name)); id.has_value())
            return id;
        constexpr std::string_view vanillaPrefix = "blockbit:";
        if (name.starts_with(vanillaPrefix))
            return textureMap().fromName(name.substr(vanillaPrefix.size()));
        return std::nullopt;
    }
    return textureMap().fromName(name);
}

std::string nameFromTexture(RuntimeID id)
{
    const auto& textures = gameRegistries().textures;
    if (textures.contains(id)) return textures.keyOf(id);
    if (const auto vanillaName = textureMap().toName(id); !vanillaName.empty())
        return "blockbit:" + vanillaName;
    return {};
}

void registerTextureKey(const std::string& key, RuntimeID id)
{
    gameRegistries().textures.registerAt(id, key, id);
}

RuntimeID firstDynamicTextureId()
{
    return static_cast<RuntimeID>(textureMap().entries.size());
}

// --- Prefab ---------------------------------------------------------------

std::optional<PrefabID> prefabFromName(std::string_view name)
{
    if (name == "player")        return PrefabID::Player;
    if (name == "pig")           return PrefabID::Pig;
    if (name == "cow")           return PrefabID::Cow;
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
        case PrefabID::Cow:           return "cow";
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
