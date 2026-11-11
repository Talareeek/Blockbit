#ifndef ITEM_BEHAVIOR_HPP
#define ITEM_BEHAVIOR_HPP

#include <SFML/System/Vector2.hpp>

#include "UUID.hpp"

class World;
struct ItemStack;
struct ItemBehavior;

// Returns true to consume one from the stack, matching the legacy
// ItemData::onUse convention. Dispatches on behavior.type; types that have
// not yet been ported return false so the caller can fall through to the
// legacy onUse lambda for one more release cycle.
bool dispatchItemUse(const ItemBehavior& behavior,
                     World& world,
                     sf::Vector2<double> mouse,
                     UUID user,
                     const ItemStack& stack);

#endif // ITEM_BEHAVIOR_HPP
