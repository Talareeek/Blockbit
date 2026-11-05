#include "../include/FlatWorldGenerator.hpp"

Tag FlatWorldLayer::serialize() const
{
    TagCompound compound;
    compound["block"] = Tag(static_cast<uint32_t>(block));
    compound["count"] = Tag(static_cast<int32_t>(count));
    return Tag(compound);
}

FlatWorldLayer FlatWorldLayer::deserialize(const Tag& tag)
{
    const TagCompound& compound = tag.get<TagCompound>();
    FlatWorldLayer layer;
    layer.block = static_cast<BlockID>(compound.at("block").get<uint32_t>());
    layer.count = static_cast<int>(compound.at("count").get<int32_t>());
    return layer;
}

Tag FlatWorldProperties::serialize() const
{
    TagCompound compound;

    TagList layers_list;
    layers_list.reserve(layers.size());
    for (const FlatWorldLayer& layer : layers)
    {
        layers_list.push_back(layer.serialize());
    }
    compound["layers"] = Tag(layers_list);

    return Tag(compound);
}

void FlatWorldProperties::deserialize(const Tag& tag)
{
    layers.clear();

    const TagCompound& compound = tag.get<TagCompound>();
    auto it = compound.find("layers");
    if (it == compound.end()) return;

    const TagList& layers_list = it->second.get<TagList>();
    layers.reserve(layers_list.size());
    for (const Tag& layer_tag : layers_list)
    {
        layers.push_back(FlatWorldLayer::deserialize(layer_tag));
    }
}

Chunk FlatWorldGenerator::generateChunk(int chunk_position)
{
    Chunk chunk;
    chunk.chunk_position = chunk_position;

    chunk.dirty = true;
    chunk.meshDirty = true;
    chunk.generated = true;

    for (int y = 0; y < CHUNK_HEIGHT; ++y)
    {
        for (int x = 0; x < CHUNK_WIDTH; ++x)
        {
            chunk.blocks[y][x] = {BlockID::Air, 0};
        }
    }

    int y = 0;
    for (const FlatWorldLayer& layer : properties.layers)
    {
        if (layer.count <= 0) continue;

        int top = std::min(CHUNK_HEIGHT, y + layer.count);

        for (int ly = y; ly < top; ++ly)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                chunk.blocks[ly][x] = {layer.block, 0};
            }
        }

        y = top;
        if (y >= CHUNK_HEIGHT) break;
    }

    return chunk;
}
