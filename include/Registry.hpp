#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Namespaced identifier such as "blockbit:stone" or "ruby_mod:magic_wand".
// Survives into save files and network handshake payloads so content stays
// uniquely addressable across packs. Format is validated by isValidRegistryKey.
using RegistryKey = std::string;

// Compact integer handed out at registration time. Only valid within a single
// process run — do NOT persist. Save files store the RegistryKey; a fresh
// RuntimeID is minted after packs finish loading.
using RuntimeID = uint16_t;

constexpr RuntimeID kInvalidRuntimeID = static_cast<RuntimeID>(-1);

// Accepts "<namespace>:<name>" where both halves are non-empty, namespace is
// [a-z0-9_]+, and name is [a-z0-9_/]+. The '/' in names is allowed so packs
// can group content by subfolder ("ruby_mod:blocks/ruby_ore").
bool isValidRegistryKey(const RegistryKey& key);

template <typename TData>
class Registry
{
public:
    struct Entry
    {
        RegistryKey key;
        TData       data;
    };

    // Inserts `data` under `key` and returns its RuntimeID. If `key` already
    // exists, overwrites data in place and returns the existing RuntimeID.
    // Later-loaded packs win — mirrors Minecraft resource-pack override order.
    RuntimeID registerEntry(RegistryKey key, TData data)
    {
        if (auto it = by_key.find(key); it != by_key.end())
        {
            entries[it->second].data = std::move(data);
            return it->second;
        }

        const RuntimeID id = static_cast<RuntimeID>(entries.size());
        entries.push_back(Entry{std::move(key), std::move(data)});
        by_key.emplace(entries[id].key, id);
        return id;
    }

    // Places `data` at a specific RuntimeID, growing the storage if needed.
    // Used during Phase 1a-A to keep RuntimeIDs aligned with the legacy
    // BlockID/ItemID enum values — so the wire protocol doesn't shift before
    // chunks switch to palette storage.
    RuntimeID registerAt(RuntimeID id, RegistryKey key, TData data)
    {
        if (entries.size() <= id) entries.resize(id + 1);
        entries[id] = Entry{std::move(key), std::move(data)};
        by_key[entries[id].key] = id;
        return id;
    }

    std::optional<RuntimeID> find(const RegistryKey& key) const
    {
        auto it = by_key.find(key);
        if (it == by_key.end()) return std::nullopt;
        return it->second;
    }

    bool contains(RuntimeID id) const { return id < entries.size() && !entries[id].key.empty(); }

    const RegistryKey& keyOf(RuntimeID id) const { return entries.at(id).key; }

    const TData& dataOf(RuntimeID id) const { return entries.at(id).data; }
    TData&       dataOf(RuntimeID id)       { return entries.at(id).data; }

    size_t size() const { return entries.size(); }

    const std::vector<Entry>& all() const { return entries; }

    void clear()
    {
        entries.clear();
        by_key.clear();
    }

private:
    std::vector<Entry>                           entries;
    std::unordered_map<RegistryKey, RuntimeID>   by_key;
};

#endif // REGISTRY_HPP
