#ifndef ITEM_DATA_COMPONENT_HPP
#define ITEM_DATA_COMPONENT_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

#include "BBT.hpp"

// Per-stack typed data, à la Minecraft 1.20.5+ data components. Fresh items
// carry none of these — a component is only attached once the stack diverges
// from the item-type default (damage > 0, custom name set, enchantment added).
// This lets `instanceID` stay unset on the common case so stacks still merge.
//
// To add a new component kind:
//   1) extend ItemComponentType,
//   2) derive from ItemDataComponent, implement the five virtuals,
//   3) register a factory with REGISTER_ITEM_DATA_COMPONENT in its .cpp.
enum class ItemComponentType : uint16_t
{
    None         = 0,
    Damage       = 1,
    CustomName   = 2,
    Enchantments = 3,
};

struct ItemDataComponent
{
    virtual ~ItemDataComponent() = default;

    virtual ItemComponentType type() const = 0;
    virtual Tag serialize() const = 0;
    virtual void deserialize(const Tag& tag) = 0;
    virtual std::unique_ptr<ItemDataComponent> clone() const = 0;
    virtual bool equals(const ItemDataComponent& other) const = 0;
};

using ItemDataComponentFactory = std::function<std::unique_ptr<ItemDataComponent>()>;

std::unordered_map<ItemComponentType, ItemDataComponentFactory>& itemDataComponentRegistry();

void registerItemDataComponent(ItemComponentType type, ItemDataComponentFactory factory);

std::unique_ptr<ItemDataComponent> makeItemDataComponent(ItemComponentType type);

// Stringify ItemComponentType for use as a key in BBT compounds. Keeping this
// as a string (not raw uint16 keys) lets BBT's compound format stay the same,
// and makes save files readable in a hex editor.
std::string itemComponentTypeKey(ItemComponentType type);
std::optional<ItemComponentType> itemComponentTypeFromKey(const std::string& key);

// Call once at startup (from main.cpp / headless entrypoint). Registers all
// built-in component factories. Idempotent.
void registerBuiltinItemDataComponents();

#define REGISTER_ITEM_DATA_COMPONENT(ClassName)                                     \
    namespace                                                                       \
    {                                                                               \
        struct ClassName##_Registrar                                                \
        {                                                                           \
            ClassName##_Registrar()                                                 \
            {                                                                       \
                registerItemDataComponent(                                          \
                    ClassName::STATIC_TYPE,                                         \
                    []() -> std::unique_ptr<ItemDataComponent>                      \
                    {                                                               \
                        return std::make_unique<ClassName>();                       \
                    });                                                             \
            }                                                                       \
        };                                                                          \
    }                                                                               \
    /* static initializer order across TUs is undefined, so we also wire these up   \
       explicitly from registerBuiltinItemDataComponents(). The static here is kept \
       as a backup for anyone linking the TU directly. */                           \
    static ClassName##_Registrar g_##ClassName##_registrar;

#endif // ITEM_DATA_COMPONENT_HPP
