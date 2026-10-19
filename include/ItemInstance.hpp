#ifndef ITEM_INSTANCE_HPP
#define ITEM_INSTANCE_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "BBT.hpp"
#include "UUID.hpp"

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
};

struct ItemInstanceData
{
    std::optional<uint32_t> durability;
    std::optional<std::string> customName;
    std::vector<Enchantment> enchantments;

    bool isEmpty() const;

    Tag serialize() const;
    static ItemInstanceData deserialize(const Tag& tag);
};

extern std::unordered_map<UUID, ItemInstanceData> itemInstances;

ItemInstanceData& getOrCreateInstance(UUID id);
const ItemInstanceData* findInstance(UUID id);
void removeInstance(UUID id);
void pruneIfEmpty(UUID id);

Tag serializeItemInstances();
void deserializeItemInstances(const Tag& tag);

#endif // ITEM_INSTANCE_HPP
