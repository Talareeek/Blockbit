#ifndef ITEM_UTILS_HPP
#define ITEM_UTILS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "BBT.hpp"
#include "Item.hpp"
#include "ItemInstance.hpp"

uint32_t getMaxDurability(ItemID id);
uint32_t getCurrentDurability(const ItemStack& stack);
std::string getDisplayName(const ItemStack& stack);

bool damageItem(ItemStack& stack, uint32_t amount);
void setCustomName(ItemStack& stack, const std::string& name);
void addEnchantment(ItemStack& stack, Enchantment enchantment);

// ---- Stack serialization ----
//
// One place that decides what goes over the wire / to disk for an ItemStack,
// and whether to trust inline instance_data on the receive side. Every stack
// serializer in the project (ItemComponent, InventoryComponent, chest/furnace
// block entities, chest/furnace/crafting views) routes through these so a
// change to the instance-data layout only needs to be made here.
//
// write_instance_data on the from* side controls whether the *local* global
// itemInstances map is updated from the payload. The server passes false when
// decoding client-sent payloads (click/craft requests) because the server is
// authoritative over instance data and must not let clients inject it. The
// client passes true because that is how it learns durability/enchants/names.
Tag stackToTag(const ItemStack& stack);
ItemStack stackFromTag(const Tag& tag, bool write_instance_data);

std::vector<uint8_t> stackToBytes(const ItemStack& stack);
std::optional<ItemStack> stackFromBytes(const std::vector<uint8_t>& buffer, bool write_instance_data);

#endif // ITEM_UTILS_HPP
