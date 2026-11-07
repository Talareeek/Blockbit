#ifndef ITEM_INSTANCE_HPP
#define ITEM_INSTANCE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "BBT.hpp"
#include "UUID.hpp"
#include "ItemDataComponent.hpp"
#include "ItemDataComponents.hpp"

// Per-instance overrides for a stack. Any component that is absent means
// "use the item-type default". An empty ItemInstanceData is therefore the
// same as having no instance at all — pruneIfEmpty exploits this to free
// memory as soon as a damaged tool is repaired or renamed back.
struct ItemInstanceData
{
    std::unordered_map<ItemComponentType, std::unique_ptr<ItemDataComponent>> components;

    bool isEmpty() const { return components.empty(); }

    bool hasComponent(ItemComponentType type) const
    {
        return components.find(type) != components.end();
    }

    template<typename T>
    T* get()
    {
        auto it = components.find(T::STATIC_TYPE);
        return it == components.end() ? nullptr : static_cast<T*>(it->second.get());
    }

    template<typename T>
    const T* get() const
    {
        auto it = components.find(T::STATIC_TYPE);
        return it == components.end() ? nullptr : static_cast<const T*>(it->second.get());
    }

    // Returns a mutable component, creating a default one if missing.
    template<typename T>
    T& ensure()
    {
        auto it = components.find(T::STATIC_TYPE);
        if (it == components.end())
        {
            auto owned = std::make_unique<T>();
            T* raw = owned.get();
            components.emplace(T::STATIC_TYPE, std::move(owned));
            return *raw;
        }
        return *static_cast<T*>(it->second.get());
    }

    template<typename T>
    void remove()
    {
        components.erase(T::STATIC_TYPE);
    }

    ItemInstanceData clone() const;
    bool operator==(const ItemInstanceData& other) const;

    Tag serialize() const;
    static ItemInstanceData deserialize(const Tag& tag);
};

extern std::unordered_map<UUID, ItemInstanceData> itemInstances;

ItemInstanceData& getOrCreateInstance(UUID id);
const ItemInstanceData* findInstance(UUID id);
ItemInstanceData* findInstanceMutable(UUID id);
void removeInstance(UUID id);
void pruneIfEmpty(UUID id);

Tag serializeItemInstances();
void deserializeItemInstances(const Tag& tag);

#endif // ITEM_INSTANCE_HPP
