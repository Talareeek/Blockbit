#include "../include/ItemDataComponents.hpp"

// ---- DamageItemComponent ----

Tag DamageItemComponent::serialize() const
{
    TagCompound compound;
    compound["damage"] = Tag(damage);
    return Tag(compound);
}

void DamageItemComponent::deserialize(const Tag& tag)
{
    const auto& compound = tag.get<TagCompound>();
    auto it = compound.find("damage");
    damage = (it != compound.end()) ? it->second.get<uint32_t>() : 0;
}

std::unique_ptr<ItemDataComponent> DamageItemComponent::clone() const
{
    return std::make_unique<DamageItemComponent>(*this);
}

bool DamageItemComponent::equals(const ItemDataComponent& other) const
{
    if (other.type() != STATIC_TYPE) return false;
    return static_cast<const DamageItemComponent&>(other).damage == damage;
}

// ---- CustomNameItemComponent ----

Tag CustomNameItemComponent::serialize() const
{
    TagCompound compound;
    compound["name"] = Tag(name);
    return Tag(compound);
}

void CustomNameItemComponent::deserialize(const Tag& tag)
{
    const auto& compound = tag.get<TagCompound>();
    auto it = compound.find("name");
    name = (it != compound.end()) ? it->second.get<std::string>() : std::string{};
}

std::unique_ptr<ItemDataComponent> CustomNameItemComponent::clone() const
{
    return std::make_unique<CustomNameItemComponent>(*this);
}

bool CustomNameItemComponent::equals(const ItemDataComponent& other) const
{
    if (other.type() != STATIC_TYPE) return false;
    return static_cast<const CustomNameItemComponent&>(other).name == name;
}

// ---- EnchantmentsItemComponent ----

Tag EnchantmentsItemComponent::serialize() const
{
    TagCompound compound;

    TagList list;
    list.reserve(entries.size());
    for (const Enchantment& e : entries)
    {
        TagCompound entry;
        entry["id"]    = Tag(static_cast<uint32_t>(e.id));
        entry["level"] = Tag(e.level);
        list.push_back(Tag(entry));
    }
    compound["list"] = Tag(list);

    return Tag(compound);
}

void EnchantmentsItemComponent::deserialize(const Tag& tag)
{
    entries.clear();

    const auto& compound = tag.get<TagCompound>();
    auto it = compound.find("list");
    if (it == compound.end()) return;

    const auto& list = it->second.get<TagList>();
    entries.reserve(list.size());

    for (const Tag& entry_tag : list)
    {
        const auto& entry = entry_tag.get<TagCompound>();
        auto id_it  = entry.find("id");
        auto lvl_it = entry.find("level");
        if (id_it == entry.end() || lvl_it == entry.end()) continue;

        Enchantment e;
        e.id    = static_cast<EnchantID>(id_it->second.get<uint32_t>());
        e.level = lvl_it->second.get<uint8_t>();
        entries.push_back(e);
    }
}

std::unique_ptr<ItemDataComponent> EnchantmentsItemComponent::clone() const
{
    return std::make_unique<EnchantmentsItemComponent>(*this);
}

bool EnchantmentsItemComponent::equals(const ItemDataComponent& other) const
{
    if (other.type() != STATIC_TYPE) return false;
    return static_cast<const EnchantmentsItemComponent&>(other).entries == entries;
}
