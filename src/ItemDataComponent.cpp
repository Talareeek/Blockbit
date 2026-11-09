#include "../include/ItemDataComponent.hpp"
#include "../include/ItemDataComponents.hpp"

std::unordered_map<ItemComponentType, ItemDataComponentFactory>& itemDataComponentRegistry()
{
    static std::unordered_map<ItemComponentType, ItemDataComponentFactory> registry;
    return registry;
}

void registerItemDataComponent(ItemComponentType type, ItemDataComponentFactory factory)
{
    itemDataComponentRegistry()[type] = std::move(factory);
}

std::unique_ptr<ItemDataComponent> makeItemDataComponent(ItemComponentType type)
{
    auto& registry = itemDataComponentRegistry();
    auto it = registry.find(type);
    if (it == registry.end()) return nullptr;
    return it->second();
}

std::string itemComponentTypeKey(ItemComponentType type)
{
    switch (type)
    {
        case ItemComponentType::Damage:       return "damage";
        case ItemComponentType::CustomName:   return "custom_name";
        case ItemComponentType::Enchantments: return "enchantments";
        case ItemComponentType::MagicEffect:  return "magic_effect";
        case ItemComponentType::Rubyness:     return "rubyness";
        case ItemComponentType::None:         return "none";
    }
    return "none";
}

std::optional<ItemComponentType> itemComponentTypeFromKey(const std::string& key)
{
    if (key == "damage")       return ItemComponentType::Damage;
    if (key == "custom_name")  return ItemComponentType::CustomName;
    if (key == "enchantments") return ItemComponentType::Enchantments;
    if (key == "magic_effect") return ItemComponentType::MagicEffect;
    if (key == "rubyness")     return ItemComponentType::Rubyness;
    return std::nullopt;
}

void registerBuiltinItemDataComponents()
{
    registerItemDataComponent(DamageItemComponent::STATIC_TYPE,
        []() { return std::make_unique<DamageItemComponent>(); });
    registerItemDataComponent(CustomNameItemComponent::STATIC_TYPE,
        []() { return std::make_unique<CustomNameItemComponent>(); });
    registerItemDataComponent(EnchantmentsItemComponent::STATIC_TYPE,
        []() { return std::make_unique<EnchantmentsItemComponent>(); });
    registerItemDataComponent(MagicEffectItemComponent::STATIC_TYPE,
        []() { return std::make_unique<MagicEffectItemComponent>(); });
    registerItemDataComponent(RubynessItemComponent::STATIC_TYPE,
        []() { return std::make_unique<RubynessItemComponent>(); });
}
