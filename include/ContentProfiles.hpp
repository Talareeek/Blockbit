#ifndef CONTENT_PROFILES_HPP
#define CONTENT_PROFILES_HPP

#include <optional>
#include <string>
#include <string_view>

#include "AssetManager.hpp"
#include "Block.hpp"
#include "Behavior.hpp"
#include "Item.hpp"
#include "Prefab.hpp"

// String ↔ enum lookups used by the JSON pack loader (PackLoader) and its
// one-shot counterpart (VanillaExporter). The strings here are the canonical
// names baked into on-disk JSON — renaming one is a format break.
namespace ContentProfiles
{

// --- Texture ---------------------------------------------------------------
std::optional<AssetManager::GameTextureID> textureFromName(std::string_view name);
std::string                                 nameFromTexture(AssetManager::GameTextureID id);

// --- Prefab ----------------------------------------------------------------
std::optional<PrefabID> prefabFromName(std::string_view name);
std::string             nameFromPrefab(PrefabID id);

// --- Tool / category / rarity ---------------------------------------------
std::optional<ToolType>     toolTypeFromName(std::string_view name);
std::string                 nameFromToolType(ToolType t);

std::optional<ItemCategory> categoryFromName(std::string_view name);
std::string                 nameFromCategory(ItemCategory c);

std::optional<ItemRarity>   rarityFromName(std::string_view name);
std::string                 nameFromRarity(ItemRarity r);

// --- Behavior types --------------------------------------------------------
std::optional<ItemBehaviorType>  itemBehaviorTypeFromName(std::string_view name);
std::string                      nameFromItemBehaviorType(ItemBehaviorType t);

std::optional<BlockBehaviorType> blockBehaviorTypeFromName(std::string_view name);
std::string                      nameFromBlockBehaviorType(BlockBehaviorType t);

std::optional<ThrowProjectileParams::Pattern> throwPatternFromName(std::string_view name);
std::string                                    nameFromThrowPattern(ThrowProjectileParams::Pattern p);

// --- Render profile --------------------------------------------------------
// Picks the {render_bounds, rect, animation} triple. Named for how the
// block visuals read: "default"/"water"/"lava"/"chest"/"furnace"/"cactus"/
// "woodcutter"/"connected"/"coreheart"/"output_bulb".
bool applyRenderProfile(RenderProperties& out, std::string_view profile);
std::string renderProfileOf(const RenderProperties& props); // best-effort inverse

// --- Callback profile ------------------------------------------------------
// Fills BlockUpdateCallbacks for a named profile. Supported names:
// "none"/"water"/"lava"/"falling"/"cactus". Returns false if profile
// is unknown.
bool applyCallbackProfile(BlockUpdateCallbacks& out, std::string_view profile);
std::string callbackProfileOf(const BlockUpdateCallbacks& cb); // best-effort inverse

} // namespace ContentProfiles

#endif // CONTENT_PROFILES_HPP
