#include "../include/StructureTemplate.hpp"

#include <stdexcept>

void StructureTemplate::toWorld(StructureRotation rot, int origin_x, int anchor_y,
                                int tx, int ty, int& wx, int& wy) const
{
    // y is unchanged by MirrorX — only x flips around the anchor column.
    const int dy = ty - anchor_y;

    const int dx = (rot == StructureRotation::MirrorX)
        ? -(tx - anchor_x)
        :  (tx - anchor_x);

    wx = origin_x + dx;
    wy = anchor_y + dy;
}

bool StructureTemplate::fromWorld(StructureRotation rot, int origin_x, int anchor_y,
                                  int wx, int wy, int& tx, int& ty) const
{
    const int dx_world = wx - origin_x;
    const int dy_world = wy - anchor_y;

    const int dx = (rot == StructureRotation::MirrorX) ? -dx_world : dx_world;

    tx = anchor_x + dx;
    ty = anchor_y + dy_world;

    return tx >= 0 && tx < width && ty >= 0 && ty < height;
}

Tag StructureTemplate::serialize() const
{
    TagCompound c;

    c["width"]    = Tag(static_cast<int32_t>(width));
    c["height"]   = Tag(static_cast<int32_t>(height));
    c["anchor_x"] = Tag(static_cast<int32_t>(anchor_x));
    c["anchor_y"] = Tag(static_cast<int32_t>(anchor_y));

    // Cells are serialized as three parallel byte arrays of length width*height
    // — block id (uint32, written as 4 bytes LE), metadata (uint16, 2 bytes LE),
    // and placement mode (uint8). That's the densest format BBT supports with
    // the existing byte-array tag, and keeps the asset files small.
    std::vector<uint8_t> ids;
    std::vector<uint8_t> metas;
    std::vector<uint8_t> modes;

    const std::size_t n = cells.size();
    ids.reserve(n * 4);
    metas.reserve(n * 2);
    modes.reserve(n);

    for (const StructureCell& cell : cells)
    {
        const uint32_t id = static_cast<uint32_t>(cell.block.id);
        ids.push_back(static_cast<uint8_t>(id & 0xFF));
        ids.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));
        ids.push_back(static_cast<uint8_t>((id >> 16) & 0xFF));
        ids.push_back(static_cast<uint8_t>((id >> 24) & 0xFF));

        const uint16_t meta = cell.block.metadata;
        metas.push_back(static_cast<uint8_t>(meta & 0xFF));
        metas.push_back(static_cast<uint8_t>((meta >> 8) & 0xFF));

        modes.push_back(static_cast<uint8_t>(cell.mode));
    }

    c["cell_ids"]   = Tag(ids);
    c["cell_metas"] = Tag(metas);
    c["cell_modes"] = Tag(modes);

    TagList be_list;
    be_list.reserve(block_entities.size());
    for (const StructureBlockEntity& be : block_entities)
    {
        TagCompound entry;
        entry["rx"] = Tag(static_cast<int32_t>(be.rx));
        entry["ry"] = Tag(static_cast<int32_t>(be.ry));
        entry["id"] = Tag(static_cast<uint32_t>(be.id));
        be_list.emplace_back(entry);
    }
    c["block_entities"] = Tag(be_list);

    TagList spawn_list;
    spawn_list.reserve(spawns.size());
    for (const StructureSpawn& sp : spawns)
    {
        TagCompound entry;
        entry["rx"]     = Tag(static_cast<int32_t>(sp.rx));
        entry["ry"]     = Tag(static_cast<int32_t>(sp.ry));
        entry["prefab"] = Tag(static_cast<int32_t>(sp.prefab));
        spawn_list.emplace_back(entry);
    }
    c["spawns"] = Tag(spawn_list);

    return Tag(c);
}

void StructureTemplate::deserialize(const Tag& tag)
{
    const TagCompound& c = tag.get<TagCompound>();

    width    = c.at("width").get<int32_t>();
    height   = c.at("height").get<int32_t>();
    anchor_x = c.at("anchor_x").get<int32_t>();
    anchor_y = c.at("anchor_y").get<int32_t>();

    const auto& ids   = c.at("cell_ids").get<std::vector<uint8_t>>();
    const auto& metas = c.at("cell_metas").get<std::vector<uint8_t>>();
    const auto& modes = c.at("cell_modes").get<std::vector<uint8_t>>();

    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    if (ids.size() != n * 4 || metas.size() != n * 2 || modes.size() != n)
        throw std::runtime_error("StructureTemplate: cell array size mismatch");

    cells.resize(n);

    for (std::size_t i = 0; i < n; i++)
    {
        const uint32_t id =
              static_cast<uint32_t>(ids[i * 4 + 0])
            | (static_cast<uint32_t>(ids[i * 4 + 1]) << 8)
            | (static_cast<uint32_t>(ids[i * 4 + 2]) << 16)
            | (static_cast<uint32_t>(ids[i * 4 + 3]) << 24);

        const uint16_t meta =
              static_cast<uint16_t>(metas[i * 2 + 0])
            | static_cast<uint16_t>(static_cast<uint16_t>(metas[i * 2 + 1]) << 8);

        cells[i].block.id       = static_cast<BlockID>(id);
        cells[i].block.metadata = meta;
        cells[i].mode           = static_cast<StructurePlacementMode>(modes[i]);
    }

    block_entities.clear();
    if (c.count("block_entities"))
    {
        const TagList& be_list = c.at("block_entities").get<TagList>();
        block_entities.reserve(be_list.size());
        for (const Tag& entry_tag : be_list)
        {
            const TagCompound& entry = entry_tag.get<TagCompound>();
            StructureBlockEntity be;
            be.rx = entry.at("rx").get<int32_t>();
            be.ry = entry.at("ry").get<int32_t>();
            be.id = static_cast<BlockEntityID>(entry.at("id").get<uint32_t>());
            block_entities.push_back(be);
        }
    }

    spawns.clear();
    if (c.count("spawns"))
    {
        const TagList& spawn_list = c.at("spawns").get<TagList>();
        spawns.reserve(spawn_list.size());
        for (const Tag& entry_tag : spawn_list)
        {
            const TagCompound& entry = entry_tag.get<TagCompound>();
            StructureSpawn sp;
            sp.rx     = entry.at("rx").get<int32_t>();
            sp.ry     = entry.at("ry").get<int32_t>();
            sp.prefab = static_cast<PrefabID>(entry.at("prefab").get<int32_t>());
            spawns.push_back(sp);
        }
    }
}
