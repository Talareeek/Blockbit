#include "../include/WorldLightView.hpp"

#include "../include/Block.hpp"
#include "../include/Chunk.hpp"
#include "../include/World.hpp"

namespace
{
    int chunkOfX(int wx)
    {
        return (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
    }

    size_t localIndex(int local_x, int wy)
    {
        return static_cast<size_t>(wy) * static_cast<size_t>(CHUNK_WIDTH)
             + static_cast<size_t>(local_x);
    }

    // Lightmap is lazy — allocated on first touch. Returning a pointer lets us
    // keep the fast path (chunk present + already sized) branch-free.
    ChunkLightData& ensureLightData(Chunk& chunk)
    {
        constexpr size_t kCells = static_cast<size_t>(CHUNK_WIDTH)
                                * static_cast<size_t>(CHUNK_HEIGHT);
        if (chunk.light.block_light.size() != kCells)
            chunk.light.block_light.assign(kCells, 0);
        if (chunk.light.sky_light.size() != kCells)
            chunk.light.sky_light.assign(kCells, 0);
        return chunk.light;
    }
}

uint8_t WorldLightView::getLight(int wx, int wy, LightChannel ch) const
{
    if (wy < 0 || wy >= CHUNK_HEIGHT) return 0;

    const int cp = chunkOfX(wx);
    auto& chunks = const_cast<World&>(world_).getChunks();
    auto it = chunks.find(cp);
    if (it == chunks.end()) return 0;

    const auto& light = it->second.light;
    const auto& arr = (ch == LightChannel::Block) ? light.block_light : light.sky_light;
    if (arr.empty()) return 0;

    const int local_x = wx - cp * CHUNK_WIDTH;
    return arr[localIndex(local_x, wy)];
}

void WorldLightView::setLight(int wx, int wy, LightChannel ch, uint8_t level)
{
    if (wy < 0 || wy >= CHUNK_HEIGHT) return;

    const int cp = chunkOfX(wx);
    auto& chunks = world_.getChunks();
    auto it = chunks.find(cp);
    if (it == chunks.end()) return;

    ChunkLightData& light = ensureLightData(it->second);
    auto& arr = (ch == LightChannel::Block) ? light.block_light : light.sky_light;

    const int local_x = wx - cp * CHUNK_WIDTH;
    const size_t idx = localIndex(local_x, wy);
    if (arr[idx] != level)
    {
        arr[idx] = level;
        light.texture_dirty = true;
    }
}

uint8_t WorldLightView::getOpacity(int wx, int wy) const
{
    Block b = const_cast<World&>(world_).getBlock(wx, wy);
    auto it = blockDatabase.find(b.id);
    if (it == blockDatabase.end()) return 15;
    // Transparent blocks (glass, leaves, water, fire…) are lighting-neutral:
    // they let light pass as if the cell were open air, regardless of the
    // tabulated light_opacity. That field only governs opaque-looking blocks.
    if (it->second.transparent) return 1;
    return it->second.light_opacity;
}

uint8_t WorldLightView::getEmission(int wx, int wy) const
{
    Block b = const_cast<World&>(world_).getBlock(wx, wy);
    auto it = blockDatabase.find(b.id);
    if (it == blockDatabase.end()) return 0;
    return it->second.light_emission;
}

bool WorldLightView::inBounds(int wx, int wy) const
{
    if (wy < 0 || wy >= CHUNK_HEIGHT) return false;
    auto& chunks = const_cast<World&>(world_).getChunks();
    return chunks.find(chunkOfX(wx)) != chunks.end();
}
