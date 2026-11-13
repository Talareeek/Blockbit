#ifndef BEHAVIOR_HPP
#define BEHAVIOR_HPP

#include <cstdint>
#include <string>
#include <variant>

// Forward decls — Behavior.hpp is included by Item.hpp, so pulling in
// Prefab.hpp (which transitively needs ItemStack and BlockID) or Block.hpp
// would introduce a cycle. The underlying types make these forward decls
// usable as value types in the params structs below.
enum class BlockID  : uint16_t;
enum class ItemID   : uint16_t;
enum class PrefabID : uint8_t;

// ----- Items ---------------------------------------------------------------

enum class ItemBehaviorType : uint8_t
{
    None = 0,
    ThrowProjectile,   // Dynamite, StabShot, NukeShot
    FluidBucket,       // Bucket, Water_Bucket — bidirectional swap
    Ignite,            // Lighter, Flint — furnace ignite OR fire placement
    ConsumeEffect      // Magic_Bottle — apply MagicEffect instance data
};

struct ThrowProjectileParams
{
    enum class Pattern : uint8_t
    {
        Single,  // one entity at player position, thrown toward mouse (Dynamite)
        Column,  // vertical column at mouse.x, stationary (StabShot)
        Arc      // many entities at mouse.xy+offset, horizontal spread (NukeShot)
    };

    // RegistryKey of the prefab the dispatcher clones at spawn time —
    // e.g. "blockbit:dynamite" or "ruby_mod:fireball". The prefab supplies
    // every component (render, explosive, physics, …); the dispatcher only
    // injects position and velocity, so changing this field actually
    // changes what gets thrown.
    std::string projectile;
    Pattern     pattern;
    uint32_t    count         = 1;   // Single: 1; Column: column height; Arc: entity count
    float       throw_speed   = 0.f; // Single: velocity multiplier toward mouse
    float       spread        = 0.f; // Arc: horizontal velocity magnitude
    float       height_offset = 0.f; // Arc: Y added to mouse before spawning
};

struct FluidBucketParams
{
    BlockID fluid;              // e.g. Water
    uint8_t source_level_value; // raw metadata value representing "source"
    ItemID  empty_variant;      // item to swap to when depositing fluid
    ItemID  filled_variant;     // item to swap to when picking up fluid
};

struct IgniteParams
{
    // Flint=true (single-use), Lighter=false (reusable tool). Consumption is
    // evaluated after either successful furnace ignite or successful fire
    // placement; unsuccessful attempts never consume.
    bool consume_on_success = false;

    // Lighter=true (can drop a Fire block onto a solid cell), Flint=false
    // (furnace-only ignition tool).
    bool can_place_fire     = false;
};

struct ConsumeEffectParams
{
    // No static params — the effect / duration / magnitude come from the
    // stack's MagicEffectItemComponent at use time.
};

struct ItemBehavior
{
    ItemBehaviorType type = ItemBehaviorType::None;
    std::variant<std::monostate,
                 ThrowProjectileParams,
                 FluidBucketParams,
                 IgniteParams,
                 ConsumeEffectParams> params;
};

// ----- Blocks --------------------------------------------------------------

enum class BlockBehaviorType : uint8_t
{
    None = 0,
    CascadeFluid,  // Water, Lava
    GravityBlock,  // Sand, Gravel
    PlantGrowth    // Cactus
};

struct CascadeFluidParams
{
    uint8_t  max_level        = 0;  // source level in metadata — also the reach
                                     // in cells since level decays by 1/cell.
    uint32_t tick_delay_ticks = 0;  // per-cell scheduleTick delay in ticks.
};

struct GravityBlockParams
{
    uint32_t fall_delay_ticks = 0;
    PrefabID falling_prefab;        // defaults filled at construction; forward
                                     // decl prevents using an enumerator here.
};

struct PlantGrowthParams
{
    uint8_t max_height = 1;
    BlockID grows_as;         // the block id written by growth (usually == self)
};

struct BlockBehavior
{
    BlockBehaviorType type = BlockBehaviorType::None;
    std::variant<std::monostate,
                 CascadeFluidParams,
                 GravityBlockParams,
                 PlantGrowthParams> params;
};

#endif // BEHAVIOR_HPP
