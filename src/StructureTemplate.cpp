#include "../include/StructureTemplate.hpp"

#include <stdexcept>

namespace
{
    uint32_t legacyIdAt(const std::vector<uint8_t>& ids, std::size_t i)
    {
        return static_cast<uint32_t>(ids[i * 4])
             | (static_cast<uint32_t>(ids[i * 4 + 1]) << 8)
             | (static_cast<uint32_t>(ids[i * 4 + 2]) << 16)
             | (static_cast<uint32_t>(ids[i * 4 + 3]) << 24);
    }

    inline uint32_t mix32(uint32_t h)
    {
        h ^= h >> 16;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13;
        h *= 0xC2B2AE35u;
        h ^= h >> 16;
        return h;
    }
}

void StructureTemplate::toWorld(StructureRotation rot, int origin_x, int placement_anchor_y,
                                int tx, int ty, int& wx, int& wy) const
{
    const bool flip_x = (rot == StructureRotation::MirrorX)
                     || (rot == StructureRotation::MirrorXY);
    const bool flip_y = (rot == StructureRotation::MirrorY)
                     || (rot == StructureRotation::MirrorXY);

    const int dx = flip_x ? -(tx - anchor_x) : (tx - anchor_x);
    const int dy = flip_y ? -(ty - anchor_y) : (ty - anchor_y);

    wx = origin_x + dx;
    wy = placement_anchor_y + dy;
}

bool StructureTemplate::fromWorld(StructureRotation rot, int origin_x, int placement_anchor_y,
                                  int wx, int wy, int& tx, int& ty) const
{
    const bool flip_x = (rot == StructureRotation::MirrorX)
                     || (rot == StructureRotation::MirrorXY);
    const bool flip_y = (rot == StructureRotation::MirrorY)
                     || (rot == StructureRotation::MirrorXY);

    const int dx_world = wx - origin_x;
    const int dy_world = wy - placement_anchor_y;

    const int dx = flip_x ? -dx_world : dx_world;
    const int dy = flip_y ? -dy_world : dy_world;

    tx = anchor_x + dx;
    ty = anchor_y + dy;

    return tx >= 0 && tx < width && ty >= 0 && ty < height;
}

Block StructureTemplate::pickFromPalette(int palette_index, uint32_t& rng_state) const
{
    if (palette_index < 0 || palette_index >= static_cast<int>(palettes.size()))
        return Block{Blocks::Air, 0};

    const StructurePalette& p = palettes[static_cast<std::size_t>(palette_index)];
    if (p.blocks.empty()) return Block{Blocks::Air, 0};

    uint32_t total = 0;
    const std::size_t n = std::min(p.blocks.size(), p.weights.size());
    for (std::size_t i = 0; i < n; i++) total += p.weights[i];
    if (total == 0) return Block{Blocks::Air, 0};

    rng_state = mix32(rng_state + 0x9E3779B9u);
    const uint32_t roll = rng_state % total;

    uint32_t acc = 0;
    for (std::size_t i = 0; i < n; i++)
    {
        acc += p.weights[i];
        if (roll < acc) return p.blocks[i];
    }
    return p.blocks.back();
}

StructureJigsawFacing StructureTemplate::rotateFacing(StructureJigsawFacing f, StructureRotation rot)
{
    const bool flip_x = (rot == StructureRotation::MirrorX)
                     || (rot == StructureRotation::MirrorXY);
    const bool flip_y = (rot == StructureRotation::MirrorY)
                     || (rot == StructureRotation::MirrorXY);

    if (flip_x)
    {
        if (f == StructureJigsawFacing::Left)  f = StructureJigsawFacing::Right;
        else if (f == StructureJigsawFacing::Right) f = StructureJigsawFacing::Left;
    }
    if (flip_y)
    {
        if (f == StructureJigsawFacing::Up)    f = StructureJigsawFacing::Down;
        else if (f == StructureJigsawFacing::Down) f = StructureJigsawFacing::Up;
    }
    return f;
}

Tag StructureTemplate::serialize() const
{
    TagCompound c;

    c["width"]    = Tag(static_cast<int32_t>(width));
    c["height"]   = Tag(static_cast<int32_t>(height));
    c["anchor_x"] = Tag(static_cast<int32_t>(anchor_x));
    c["anchor_y"] = Tag(static_cast<int32_t>(anchor_y));

    // Core cell arrays unchanged on disk — three parallel byte arrays packed
    // dense. Two new optional arrays (palette index + probability) are written
    // only when any cell actually uses them so old-style templates stay
    // byte-for-byte compatible.
    TagList ids;
    std::vector<uint8_t> metas;
    std::vector<uint8_t> modes;
    std::vector<uint8_t> palette_indices; // 2 bytes LE per cell, 0xFFFF = none
    std::vector<uint8_t> probabilities;   // 1 byte per cell

    const std::size_t n = cells.size();
    ids.reserve(n);
    metas.reserve(n * 2);
    modes.reserve(n);
    palette_indices.reserve(n * 2);
    probabilities.reserve(n);

    bool any_palette     = false;
    bool any_probability = false;

    for (const StructureCell& cell : cells)
    {
        ids.emplace_back(cell.block.id);

        const uint16_t meta = cell.block.metadata;
        metas.push_back(static_cast<uint8_t>(meta & 0xFF));
        metas.push_back(static_cast<uint8_t>((meta >> 8) & 0xFF));

        modes.push_back(static_cast<uint8_t>(cell.mode));

        const uint16_t pal = (cell.palette < 0) ? 0xFFFFu : static_cast<uint16_t>(cell.palette);
        palette_indices.push_back(static_cast<uint8_t>(pal & 0xFF));
        palette_indices.push_back(static_cast<uint8_t>((pal >> 8) & 0xFF));
        if (cell.palette >= 0) any_palette = true;

        probabilities.push_back(cell.probability);
        if (cell.probability != 255) any_probability = true;
    }

    c["cell_ids"]   = Tag(ids);
    c["cell_metas"] = Tag(metas);
    c["cell_modes"] = Tag(modes);

    if (any_palette)     c["cell_palettes"]      = Tag(palette_indices);
    if (any_probability) c["cell_probabilities"] = Tag(probabilities);

    if (!palettes.empty())
    {
        TagList pal_list;
        pal_list.reserve(palettes.size());
        for (const StructurePalette& p : palettes)
        {
            TagCompound entry;
            TagList pids;
            std::vector<uint8_t> pmetas;
            std::vector<uint8_t> pweights;
            const std::size_t m = std::min(p.blocks.size(), p.weights.size());
            pids.reserve(m);
            pmetas.reserve(m * 2);
            pweights.reserve(m * 2);
            for (std::size_t i = 0; i < m; i++)
            {
                pids.emplace_back(p.blocks[i].id);

                const uint16_t meta = p.blocks[i].metadata;
                pmetas.push_back(static_cast<uint8_t>(meta & 0xFF));
                pmetas.push_back(static_cast<uint8_t>((meta >> 8) & 0xFF));

                const uint16_t w = p.weights[i];
                pweights.push_back(static_cast<uint8_t>(w & 0xFF));
                pweights.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
            }
            entry["ids"]     = Tag(pids);
            entry["metas"]   = Tag(pmetas);
            entry["weights"] = Tag(pweights);
            pal_list.emplace_back(entry);
        }
        c["palettes"] = Tag(pal_list);
    }

    TagList be_list;
    be_list.reserve(block_entities.size());
    for (const StructureBlockEntity& be : block_entities)
    {
        TagCompound entry;
        entry["rx"] = Tag(static_cast<int32_t>(be.rx));
        entry["ry"] = Tag(static_cast<int32_t>(be.ry));
        entry["id"] = Tag(static_cast<uint32_t>(be.id));
        if (!be.loot_table.empty()) entry["loot"] = Tag(be.loot_table);
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
        entry["prefab"] = Tag(sp.prefab);
        spawn_list.emplace_back(entry);
    }
    c["spawns"] = Tag(spawn_list);

    if (!jigsaws.empty())
    {
        TagList j_list;
        j_list.reserve(jigsaws.size());
        for (const StructureJigsaw& j : jigsaws)
        {
            TagCompound entry;
            entry["rx"]        = Tag(static_cast<int32_t>(j.rx));
            entry["ry"]        = Tag(static_cast<int32_t>(j.ry));
            entry["facing"]    = Tag(static_cast<uint32_t>(j.facing));
            entry["connector"] = Tag(j.connector);
            entry["pool"]      = Tag(j.pool);
            j_list.emplace_back(entry);
        }
        c["jigsaws"] = Tag(j_list);
    }

    if (allow_mirror_y) c["allow_mirror_y"] = Tag(static_cast<uint8_t>(1));

    return Tag(c);
}

void StructureTemplate::deserialize(const Tag& tag)
{
    const TagCompound& c = tag.get<TagCompound>();

    width    = c.at("width").get<int32_t>();
    height   = c.at("height").get<int32_t>();
    anchor_x = c.at("anchor_x").get<int32_t>();
    anchor_y = c.at("anchor_y").get<int32_t>();

    const Tag& ids_tag = c.at("cell_ids");
    const auto& metas = c.at("cell_metas").get<std::vector<uint8_t>>();
    const auto& modes = c.at("cell_modes").get<std::vector<uint8_t>>();

    const std::size_t n = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    const TagList* named_ids = ids_tag.type() == TagType::T_LIST ? &ids_tag.get<TagList>() : nullptr;
    const std::vector<uint8_t>* numeric_ids = ids_tag.type() == TagType::T_BYTEARRAY
        ? &ids_tag.get<std::vector<uint8_t>>() : nullptr;
    if ((!named_ids && (!numeric_ids || numeric_ids->size() != n * 4))
        || (named_ids && named_ids->size() != n)
        || metas.size() != n * 2 || modes.size() != n)
        throw std::runtime_error("StructureTemplate: cell array size mismatch");

    cells.resize(n);

    const std::vector<uint8_t>* pal_bytes = nullptr;
    const std::vector<uint8_t>* prob_bytes = nullptr;
    if (c.count("cell_palettes"))
    {
        const auto& v = c.at("cell_palettes").get<std::vector<uint8_t>>();
        if (v.size() == n * 2) pal_bytes = &v;
    }
    if (c.count("cell_probabilities"))
    {
        const auto& v = c.at("cell_probabilities").get<std::vector<uint8_t>>();
        if (v.size() == n) prob_bytes = &v;
    }

    for (std::size_t i = 0; i < n; i++)
    {
        const uint16_t meta =
              static_cast<uint16_t>(metas[i * 2 + 0])
            | static_cast<uint16_t>(static_cast<uint16_t>(metas[i * 2 + 1]) << 8);

        cells[i].block.id       = named_ids
            ? (*named_ids)[i].get<std::string>()
            : blockKeyFromLegacySave(legacyIdAt(*numeric_ids, i));
        cells[i].block.metadata = meta;
        cells[i].mode           = static_cast<StructurePlacementMode>(modes[i]);

        if (pal_bytes)
        {
            const uint16_t raw =
                  static_cast<uint16_t>((*pal_bytes)[i * 2 + 0])
                | static_cast<uint16_t>(static_cast<uint16_t>((*pal_bytes)[i * 2 + 1]) << 8);
            cells[i].palette = (raw == 0xFFFFu) ? static_cast<int16_t>(-1)
                                                : static_cast<int16_t>(raw);
        }
        else cells[i].palette = -1;

        cells[i].probability = prob_bytes ? (*prob_bytes)[i] : 255;
    }

    palettes.clear();
    if (c.count("palettes"))
    {
        const TagList& pal_list = c.at("palettes").get<TagList>();
        palettes.reserve(pal_list.size());
        for (const Tag& entry_tag : pal_list)
        {
            const TagCompound& entry = entry_tag.get<TagCompound>();
            StructurePalette p;
            const Tag& pids_tag  = entry.at("ids");
            const TagList* named_pids = pids_tag.type() == TagType::T_LIST ? &pids_tag.get<TagList>() : nullptr;
            const std::vector<uint8_t>* numeric_pids = pids_tag.type() == TagType::T_BYTEARRAY
                ? &pids_tag.get<std::vector<uint8_t>>() : nullptr;
            const auto& pmetas   = entry.at("metas").get<std::vector<uint8_t>>();
            const auto& pweights = entry.at("weights").get<std::vector<uint8_t>>();
            const std::size_t m = named_pids ? named_pids->size()
                : (numeric_pids ? numeric_pids->size() / 4 : 0);
            p.blocks.reserve(m);
            p.weights.reserve(m);
            for (std::size_t i = 0; i < m; i++)
            {
                const uint16_t meta =
                      static_cast<uint16_t>(pmetas[i * 2 + 0])
                    | static_cast<uint16_t>(static_cast<uint16_t>(pmetas[i * 2 + 1]) << 8);

                const uint16_t w =
                      static_cast<uint16_t>(pweights[i * 2 + 0])
                    | static_cast<uint16_t>(static_cast<uint16_t>(pweights[i * 2 + 1]) << 8);

                const BlockKey key = named_pids ? (*named_pids)[i].get<std::string>()
                    : blockKeyFromLegacySave(legacyIdAt(*numeric_pids, i));
                p.blocks.push_back(Block{key, meta});
                p.weights.push_back(w);
            }
            palettes.push_back(std::move(p));
        }
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
            if (entry.count("loot")) be.loot_table = entry.at("loot").get<std::string>();
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
            sp.prefab = entry.at("prefab").get<std::string>();
            spawns.push_back(sp);
        }
    }

    jigsaws.clear();
    if (c.count("jigsaws"))
    {
        const TagList& j_list = c.at("jigsaws").get<TagList>();
        jigsaws.reserve(j_list.size());
        for (const Tag& entry_tag : j_list)
        {
            const TagCompound& entry = entry_tag.get<TagCompound>();
            StructureJigsaw j;
            j.rx        = entry.at("rx").get<int32_t>();
            j.ry        = entry.at("ry").get<int32_t>();
            j.facing    = static_cast<StructureJigsawFacing>(entry.at("facing").get<uint32_t>());
            j.connector = entry.at("connector").get<std::string>();
            j.pool      = entry.at("pool").get<std::string>();
            jigsaws.push_back(j);
        }
    }

    allow_mirror_y = c.count("allow_mirror_y") && c.at("allow_mirror_y").get<uint8_t>() != 0;
}
