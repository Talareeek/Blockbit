#ifndef ITEM_DATA_COMPONENTS_HPP
#define ITEM_DATA_COMPONENTS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ItemDataComponent.hpp"

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

#endif // ITEM_DATA_COMPONENTS_HPP
