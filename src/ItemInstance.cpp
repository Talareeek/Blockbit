#include "../include/ItemInstance.hpp"

std::unordered_map<UUID, ItemInstanceData> itemInstances;

bool ItemInstanceData::isEmpty() const
{
    return !durability.has_value()
        && !customName.has_value()
        && enchantments.empty();
}

Tag ItemInstanceData::serialize() const
{
    TagCompound compound;

    if (durability.has_value())
    {
        compound["durability"] = Tag(*durability);
    }

    if (customName.has_value())
    {
        compound["custom_name"] = Tag(*customName);
    }

    TagList enchant_list;
    enchant_list.reserve(enchantments.size());
    for (const Enchantment& e : enchantments)
    {
        TagCompound entry;
        entry["id"] = Tag(static_cast<uint32_t>(e.id));
        entry["level"] = Tag(e.level);
        enchant_list.push_back(Tag(entry));
    }
    compound["enchantments"] = Tag(enchant_list);

    return Tag(compound);
}

ItemInstanceData ItemInstanceData::deserialize(const Tag& tag)
{
    ItemInstanceData out;

    const auto& compound = tag.get<TagCompound>();

    if (auto it = compound.find("durability"); it != compound.end())
    {
        out.durability = it->second.get<uint32_t>();
    }

    if (auto it = compound.find("custom_name"); it != compound.end())
    {
        out.customName = it->second.get<std::string>();
    }

    if (auto it = compound.find("enchantments"); it != compound.end())
    {
        const auto& list = it->second.get<TagList>();
        out.enchantments.reserve(list.size());
        for (const Tag& entry_tag : list)
        {
            const auto& entry = entry_tag.get<TagCompound>();
            auto id_it = entry.find("id");
            auto lvl_it = entry.find("level");
            if (id_it == entry.end() || lvl_it == entry.end()) continue;

            Enchantment e;
            e.id = static_cast<EnchantID>(id_it->second.get<uint32_t>());
            e.level = lvl_it->second.get<uint8_t>();
            out.enchantments.push_back(e);
        }
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
