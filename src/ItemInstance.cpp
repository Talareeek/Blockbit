#include "../include/ItemInstance.hpp"

std::unordered_map<UUID, ItemInstanceData> itemInstances;

ItemInstanceData ItemInstanceData::clone() const
{
    ItemInstanceData out;
    out.components.reserve(components.size());
    for (const auto& [type, comp] : components)
    {
        out.components.emplace(type, comp->clone());
    }
    return out;
}

bool ItemInstanceData::operator==(const ItemInstanceData& other) const
{
    if (components.size() != other.components.size()) return false;

    for (const auto& [type, comp] : components)
    {
        auto it = other.components.find(type);
        if (it == other.components.end()) return false;
        if (!comp->equals(*it->second)) return false;
    }
    return true;
}

Tag ItemInstanceData::serialize() const
{
    TagCompound compound;

    for (const auto& [type, comp] : components)
    {
        compound[itemComponentTypeKey(type)] = comp->serialize();
    }

    return Tag(compound);
}

ItemInstanceData ItemInstanceData::deserialize(const Tag& tag)
{
    ItemInstanceData out;

    const auto& compound = tag.get<TagCompound>();

    for (const auto& [key, sub_tag] : compound)
    {
        auto type = itemComponentTypeFromKey(key);
        if (!type) continue;

        auto comp = makeItemDataComponent(*type);
        if (!comp) continue;

        comp->deserialize(sub_tag);
        out.components[*type] = std::move(comp);
    }

    return out;
}

ItemInstanceData& getOrCreateInstance(UUID id)
{
    return itemInstances[id];
}

const ItemInstanceData* findInstance(UUID id)
{
    auto it = itemInstances.find(id);
    if (it == itemInstances.end()) return nullptr;
    return &it->second;
}

ItemInstanceData* findInstanceMutable(UUID id)
{
    auto it = itemInstances.find(id);
    if (it == itemInstances.end()) return nullptr;
    return &it->second;
}

void removeInstance(UUID id)
{
    itemInstances.erase(id);
}

void pruneIfEmpty(UUID id)
{
    auto it = itemInstances.find(id);
    if (it == itemInstances.end()) return;
    if (it->second.isEmpty()) itemInstances.erase(it);
}

Tag serializeItemInstances()
{
    TagCompound compound;

    for (const auto& [id, data] : itemInstances)
    {
        compound[id.toString()] = data.serialize();
    }

    return Tag(compound);
}

void deserializeItemInstances(const Tag& tag)
{
    itemInstances.clear();

    const auto& compound = tag.get<TagCompound>();

    for (const auto& [id_str, data_tag] : compound)
    {
        auto parsed = uuidFromString(id_str);
        if (!parsed.has_value()) continue;

        itemInstances.emplace(*parsed, ItemInstanceData::deserialize(data_tag));
    }
}
