#include "../include/ContentProfiles.hpp"
#include "../include/GameRegistries.hpp"

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
void water_on_post_placement(BlockUpdateContext&, sf::Vector2i, uint8_t);
void water_on_neighbor_changed(BlockUpdateContext&, sf::Vector2i, sf::Vector2i);
void water_on_scheduled_tick(BlockUpdateContext&, sf::Vector2i);
void lava_on_place(BlockUpdateContext&, sf::Vector2i);
void lava_on_post_placement(BlockUpdateContext&, sf::Vector2i, uint8_t);
void lava_on_neighbor_changed(BlockUpdateContext&, sf::Vector2i, sf::Vector2i);
void lava_on_scheduled_tick(BlockUpdateContext&, sf::Vector2i);
void cactus_on_post_placement(BlockUpdateContext&, sf::Vector2i, uint8_t);
void cactus_on_random_tick(BlockUpdateContext&, sf::Vector2i);
void leaves_on_random_tick(BlockUpdateContext&, sf::Vector2i);
void sapling_on_random_tick(BlockUpdateContext&, sf::Vector2i);
}

namespace ContentProfiles
{

using namespace blockbit_callbacks;

std::optional<RuntimeID> textureFromName(std::string_view name)
{
    if (name.find(':') == std::string_view::npos)
        return std::nullopt;
    return gameRegistries().textures.find(std::string(name));
}

std::string nameFromTexture(RuntimeID id)
{
    const auto& textures = gameRegistries().textures;
    return textures.contains(id) ? textures.keyOf(id) : std::string{};
}

void registerTextureKey(const std::string& key, RuntimeID id)
{
    gameRegistries().textures.registerAt(id, key, id);
}

RuntimeID firstDynamicTextureId()
{
    return RuntimeID{0};
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
    if (profile == "logic_gate")
    {
        out.render_bounds = default_render_bounds;
        out.rect = logic_gate_render_rect;
        out.animation = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    if (profile == "ruby_wire")
    {
        out.render_bounds = default_render_bounds;
        out.rect          = make_ruby_wire_render_rect({});
        out.animation     = [](Block) -> BlockAnimation { return {}; };
        return true;
    }
    return false;
}

std::string renderProfileOf(const RenderProperties& /*props*/)
{
    // std::function has no comparable identity — the exporter uses an
    // explicit BlockKey → profile table instead. Returning "default" here
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
        out.onPostPlacement    = water_on_post_placement;
        out.onNeighborChanged  = water_on_neighbor_changed;
        out.onScheduledTick    = water_on_scheduled_tick;
        return true;
    }
    if (profile == "lava")
    {
        out.onPlace            = lava_on_place;
        out.onPostPlacement    = lava_on_post_placement;
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
    if (profile == "leaves")
    {
        out.onRandomTick = leaves_on_random_tick;
        return true;
    }
    if (profile == "sapling")
    {
        out.onRandomTick = sapling_on_random_tick;
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
    if (cb.onRandomTick == leaves_on_random_tick) return "leaves";
    if (cb.onRandomTick == sapling_on_random_tick) return "sapling";
    return "none";
}

} // namespace ContentProfiles
