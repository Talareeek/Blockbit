#include "../include/GameRegistries.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "../include/PackLoader.hpp"
#include "../include/VanillaExporter.hpp"

GameRegistries& gameRegistries()
{
    static GameRegistries instance;
    return instance;
}

namespace
{

// Maps BlockID enum value → snake_case key segment. The array index equals the
// enum value, so BlockID::Stone (== 1) → kBlockNames[1] == "stone". Order must
// exactly match the enum in include/Block.hpp; a mismatch is a bug, not a
// migration — fix the table.
constexpr std::array<std::string_view, 45> kBlockNames =
{
    "air",                    // 0  BlockID::Air
    "stone",                  // 1  BlockID::Stone
    "grass",                  // 2  BlockID::Grass
    "dirt",                   // 3  BlockID::Dirt
    "cobblestone",            // 4  BlockID::Cobblestone
    "obsidian",               // 5  BlockID::Obsidian
    "bedrock",                // 6  BlockID::Bedrock
    "water",                  // 7  BlockID::Water
    "lava",                   // 8  BlockID::Lava
    "coal_ore",               // 9
    "iron_ore",               // 10
    "gold_ore",               // 11
    "diamond_ore",            // 12
    "ruby_ore",               // 13
    "iron_block",             // 14
    "gold_block",             // 15
    "ruby_block",             // 16
    "coal_block",             // 17
    "oak_log",                // 18
    "oak_leaves",             // 19
    "oak_planks",             // 20
    "birch_log",              // 21
    "birch_leaves",           // 22
    "spruce_log",             // 23
    "spruce_leaves",          // 24
    "woodcutter",             // 25
    "fire",                   // 26
    "sand",                   // 27
    "coarse_dirt",            // 28
    "snow",                   // 29
    "chest",                  // 30
    "bricks",                 // 31
    "furnace",                // 32
    "workbench",              // 33
    "gravel",                 // 34
    "cactus",                 // 35
    "dead_bush",              // 36
    "smooth_stone",           // 37
    "diorite",                // 38
    "granite",                // 39
    "glass",                  // 40
    "output_bulb",            // 41
    "ruby_wire",              // 42
    "coreheart_of_blockbit",  // 43
    "upgrade_table"           // 44
};
static_assert(kBlockNames.size() == static_cast<std::size_t>(BlockID::Upgrade_Table) + 1,
              "kBlockNames size must match BlockID enum range exactly");

// Maps ItemID enum value → snake_case key segment. ItemID::None is value 0 and
// represents an empty slot sentinel — keyed as "air" to match the Minecraft
// convention where the empty item and the air block share a name.
constexpr std::array<std::string_view, 76> kItemNames =
{
    "air",                    // 0  ItemID::None
    "stone",                  // 1
    "grass",                  // 2
    "dirt",                   // 3
    "cobblestone",            // 4
    "obsidian",               // 5
    "bedrock",                // 6
    "dynamite",               // 7
    "stab_shot",              // 8
    "nuke_shot",              // 9
    "coal_ore",               // 10
    "iron_ore",               // 11
    "gold_ore",               // 12
    "diamond_ore",            // 13
    "ruby_ore",               // 14
    "iron_block",             // 15
    "gold_block",             // 16
    "ruby_block",             // 17
    "coal_block",             // 18
    "coal",                   // 19
    "raw_iron",               // 20
    "raw_gold",               // 21
    "diamond",                // 22
    "oak_log",                // 23
    "oak_leaves",             // 24
    "oak_planks",             // 25
    "birch_log",              // 26
    "birch_leaves",           // 27
    "spruce_log",             // 28
    "spruce_leaves",          // 29
    "bucket",                 // 30
    "water_bucket",           // 31
    "woodcutter",             // 32
    "lighter",                // 33
    "sand",                   // 34
    "coarse_dirt",            // 35
    "snow",                   // 36
    "gravel",                 // 37
    "flint",                  // 38
    "wooden_pickaxe",         // 39
    "stone_pickaxe",          // 40
    "iron_pickaxe",           // 41
    "gold_pickaxe",           // 42
    "diamond_pickaxe",        // 43
    "ruby_pickaxe",           // 44
    "wooden_axe",             // 45
    "stone_axe",              // 46
    "iron_axe",               // 47
    "gold_axe",               // 48
    "diamond_axe",            // 49
    "ruby_axe",               // 50
    "wooden_shovel",          // 51
    "stone_shovel",           // 52
    "iron_shovel",            // 53
    "gold_shovel",            // 54
    "diamond_shovel",         // 55
    "ruby_shovel",            // 56
    "ruby",                   // 57
    "stick",                  // 58
    "chest",                  // 59
    "bricks",                 // 60
    "furnace",                // 61
    "iron_ingot",             // 62
    "gold_ingot",             // 63
    "workbench",              // 64
    "cactus",                 // 65
    "dead_bush",              // 66
    "smooth_stone",           // 67
    "diorite",                // 68
    "granite",                // 69
    "glass",                  // 70
    "output_bulb",            // 71
    "ruby_wire",              // 72
    "coreheart_of_blockbit",  // 73
    "upgrade_table",          // 74
    "magic_bottle"            // 75
};
static_assert(kItemNames.size() == static_cast<std::size_t>(ItemID::Magic_Bottle) + 1,
              "kItemNames size must match ItemID enum range exactly");

constexpr std::string_view kVanillaNamespace = "blockbit:";

std::string vanillaKey(std::string_view name)
{
    std::string out;
    out.reserve(kVanillaNamespace.size() + name.size());
    out.append(kVanillaNamespace);
    out.append(name);
    return out;
}

} // namespace

void registerVanillaContent()
{
    auto& reg = gameRegistries();

    reg.blocks.clear();
    reg.items.clear();

    // Blocks — iterate the hardcoded blockDatabase and mirror each entry. The
    // enum value becomes the RuntimeID, preserving every existing serialized
    // byte on disk and on the wire until Phase 1a-C flips BlockID itself.
    for (std::size_t i = 0; i < kBlockNames.size(); ++i)
    {
        const auto id = static_cast<BlockID>(i);
        auto it = blockDatabase.find(id);
        if (it == blockDatabase.end()) continue;

        reg.blocks.registerAt(
            static_cast<RuntimeID>(i),
            vanillaKey(kBlockNames[i]),
            it->second);
    }

    for (std::size_t i = 0; i < kItemNames.size(); ++i)
    {
        const auto id = static_cast<ItemID>(i);
        auto it = itemDatabase.find(id);
        if (it == itemDatabase.end()) continue;

        reg.items.registerAt(
            static_cast<RuntimeID>(i),
            vanillaKey(kItemNames[i]),
            it->second);
    }

    // One-shot exporter hook — set BLOCKBIT_EXPORT_VANILLA=<dir> to dump the
    // live blockDatabase / itemDatabase as JSON into <dir> and exit. The hook
    // runs AFTER mirroring to registries so both sources of truth agree.
    if (const char* export_dir = std::getenv("BLOCKBIT_EXPORT_VANILLA"))
    {
        auto stats = VanillaExporter::exportTo(export_dir);
        std::cout << "[VanillaExporter] Wrote "
                  << stats.blocks_written << " blocks, "
                  << stats.items_written  << " items to " << export_dir << '\n';
        std::exit(0);
    }

    // Phase 1c probe: try to load resources/packs/blockbit/. For now this
    // only augments the hardcoded db with any entries that aren't already
    // present and logs a summary so we can see the loader works end-to-end.
    // Phase 1c-E will replace the hardcoded path with pure pack loading.
    auto pack = PackLoader::loadPack("resources/packs/blockbit");
    if (!pack.ok)
    {
        std::cerr << "[Pack] Skipped loading blockbit pack: " << pack.error << '\n';
    }
    else
    {
        std::cout << "[Pack] Loaded '" << pack.manifest.id
                  << "' v" << pack.manifest.version
                  << " — " << pack.blocks.size() << " blocks, "
                  << pack.items.size() << " items\n";
        PackLoader::applyToDatabases(pack);

        // Re-mirror the (now pack-driven) databases into the registries so
        // keys and data agree across every lookup path.
        reg.blocks.clear();
        reg.items.clear();
        for (std::size_t i = 0; i < kBlockNames.size(); ++i)
        {
            const auto id = static_cast<BlockID>(i);
            if (auto it = blockDatabase.find(id); it != blockDatabase.end())
                reg.blocks.registerAt(static_cast<RuntimeID>(i),
                                      vanillaKey(kBlockNames[i]),
                                      it->second);
        }
        for (std::size_t i = 0; i < kItemNames.size(); ++i)
        {
            const auto id = static_cast<ItemID>(i);
            if (auto it = itemDatabase.find(id); it != itemDatabase.end())
                reg.items.registerAt(static_cast<RuntimeID>(i),
                                     vanillaKey(kItemNames[i]),
                                     it->second);
        }
    }
}
