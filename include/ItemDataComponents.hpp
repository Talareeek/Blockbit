#ifndef ITEM_DATA_COMPONENTS_HPP
#define ITEM_DATA_COMPONENTS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ItemDataComponent.hpp"
#include "EffectsComponent.hpp"

enum class EnchantID : uint32_t
{
    None = 0,
    Sharpness,
    Efficiency,
    Unbreaking,
    Fortune
};

struct Enchantment
{
    EnchantID id = EnchantID::None;
    uint8_t level = 0;

    bool operator==(const Enchantment& other) const
    {
        return id == other.id && level == other.level;
    }
};

// Uses-used counter — defaults to 0 for a fresh item. getCurrentDurability()
// returns maxDurability − damage. We store "damage" (not "remaining") so the
// default is the elidable one, matching MC 1.20.5+.
struct DamageItemComponent : public ItemDataComponent
{
    static constexpr ItemComponentType STATIC_TYPE = ItemComponentType::Damage;

    uint32_t damage = 0;

    ItemComponentType type() const override { return STATIC_TYPE; }
    Tag serialize() const override;
    void deserialize(const Tag& tag) override;
    std::unique_ptr<ItemDataComponent> clone() const override;
    bool equals(const ItemDataComponent& other) const override;
};

struct CustomNameItemComponent : public ItemDataComponent
{
    static constexpr ItemComponentType STATIC_TYPE = ItemComponentType::CustomName;

    std::string name;

    ItemComponentType type() const override { return STATIC_TYPE; }
    Tag serialize() const override;
    void deserialize(const Tag& tag) override;
    std::unique_ptr<ItemDataComponent> clone() const override;
    bool equals(const ItemDataComponent& other) const override;
};

struct EnchantmentsItemComponent : public ItemDataComponent
{
    static constexpr ItemComponentType STATIC_TYPE = ItemComponentType::Enchantments;

    std::vector<Enchantment> entries;

    ItemComponentType type() const override { return STATIC_TYPE; }
    Tag serialize() const override;
    void deserialize(const Tag& tag) override;
    std::unique_ptr<ItemDataComponent> clone() const override;
    bool equals(const ItemDataComponent& other) const override;
};

// Rubyness: ruby-item attunement with 24 levels, one per Greek letter
// (alpha..omega). Baseline is Alpha; a stack at Omega gets the full 10x boost.
// The component is only materialised on a stack when its level diverges from
// Alpha, so unupgraded ruby stacks still merge like any other block/material.
inline constexpr uint8_t RUBYNESS_MIN_LEVEL = 1;   // alpha
inline constexpr uint8_t RUBYNESS_MAX_LEVEL = 24;  // omega
inline constexpr float   RUBYNESS_OMEGA_MULTIPLIER = 10.0f;

struct RubynessItemComponent : public ItemDataComponent
{
    static constexpr ItemComponentType STATIC_TYPE = ItemComponentType::Rubyness;

    uint8_t level = RUBYNESS_MIN_LEVEL;

    ItemComponentType type() const override { return STATIC_TYPE; }
    Tag serialize() const override;
    void deserialize(const Tag& tag) override;
    std::unique_ptr<ItemDataComponent> clone() const override;
    bool equals(const ItemDataComponent& other) const override;
};

// Payload for a single Magic Bottle. Multiple bottles share one ItemID
// (Magic_Bottle); the effect, duration and strength live here so a stack with
// this component is uniquely identified by its instance UUID.
struct MagicEffectItemComponent : public ItemDataComponent
{
    static constexpr ItemComponentType STATIC_TYPE = ItemComponentType::MagicEffect;

    EffectID effect     = EffectID::None;
    float    duration   = 0.0f;
    uint32_t magnitude  = 1;

    ItemComponentType type() const override { return STATIC_TYPE; }
    Tag serialize() const override;
    void deserialize(const Tag& tag) override;
    std::unique_ptr<ItemDataComponent> clone() const override;
    bool equals(const ItemDataComponent& other) const override;
};

#endif // ITEM_DATA_COMPONENTS_HPP
