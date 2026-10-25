#ifndef CHUNK_LIGHT_DATA_HPP
#define CHUNK_LIGHT_DATA_HPP

#include <cstdint>
#include <vector>

// Per-chunk lightmap. Lives next to `blocks` on Chunk, flat arrays so BFS can
// stride through them with no indirection and so the client can hand them to
// sf::Texture::update() without a repack.
//
// Both channels are packed [y * width + x], same as `Chunk::packLocal`. Values
// are 0..15.
//
//   block_light  — torches, lava, fire. Independent of time of day.
//   sky_light    — geometric exposure to the sky, 0..15. Time-of-day scaling
//                  happens in the shader (uniform), not here.
//
// initialized is set by WorldLightView once the chunk has been lit at least
// once. texture_dirty tells the renderer the sf::Texture needs re-uploading
// (one flag per chunk is enough — sub-rect updates aren't worth the extra
// bookkeeping for a 16x256 texture).
struct ChunkLightData
{
    std::vector<uint8_t> block_light;
    std::vector<uint8_t> sky_light;
    bool initialized   = false;
    bool texture_dirty = true;
};

#endif // CHUNK_LIGHT_DATA_HPP
