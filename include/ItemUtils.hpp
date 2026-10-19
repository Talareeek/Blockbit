#ifndef ITEM_UTILS_HPP
#define ITEM_UTILS_HPP

#include <cstdint>
#include <string>

#include "Item.hpp"
#include "ItemInstance.hpp"

uint32_t getMaxDurability(ItemID id);
uint32_t getCurrentDurability(const ItemStack& stack);
std::string getDisplayName(const ItemStack& stack);

bool damageItem(ItemStack& stack, uint32_t amount);
void setCustomName(ItemStack& stack, const std::string& name);
void addEnchantment(ItemStack& stack, Enchantment enchantment);

#endif // ITEM_UTILS_HPP
