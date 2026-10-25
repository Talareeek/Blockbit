#ifndef LIGHTING_SYSTEM_HPP
#define LIGHTING_SYSTEM_HPP

#include <cstdint>
#include <deque>

// 2D block-based lighting (BFS, FIFO, deterministic).
//
// The module is deliberately decoupled from World/Chunk/SFML so the exact
// same code runs on the server (gameplay data) and the client (render data).
// Determinism comes from:
//   * strict FIFO queues (std::deque, push_back/pop_front),
//   * no unordered_map iteration in the hot path,
//   * all world lookups go through LightView, which the caller provides.
//
// Opacity model (uint8_t 0..15):
//   <=1 -> air-equivalent (loses 1 per step).
//   >1  -> solid block (loses 2 per step).
//
// Light propagates through both air and solid blocks — blocks just attenuate
// twice as fast. Sky light is seeded on every air cell that has no block
// above it in its column; from there the same BFS fills the rest of the
// world (including caves, by leaking slowly through the rock above).

enum class LightChannel : uint8_t
{
    Block = 0,
    Sky   = 1
};

struct LightNode
{
    int     wx;
    int     wy;
    uint8_t level;
};

// Minimum the lighting core needs from the world. The caller writes an
// adapter (WorldLightView on top of World, or a fake 2D grid in tests).
struct LightView
{
    virtual uint8_t getLight(int wx, int wy, LightChannel ch) const = 0;
    virtual void    setLight(int wx, int wy, LightChannel ch, uint8_t level) = 0;

    // Opacity 1 is baseline air; 15 is fully blocking.
    virtual uint8_t getOpacity(int wx, int wy) const = 0;

    // 0 = not an emitter. Only read for Block channel.
    virtual uint8_t getEmission(int wx, int wy) const = 0;

    // False -> skip the cell (edge of loaded world).
    virtual bool    inBounds(int wx, int wy) const = 0;

    virtual ~LightView() = default;
};

// Classic FIFO BFS. Seeds are starting points: for each one the view's current
// light must already be >= seed.level (caller is responsible for writing it).
// The function spreads light to neighbors, honoring opacity and the sky
// vertical-no-loss rule.
void propagateLight(LightView& view,
                    std::deque<LightNode> seeds,
                    LightChannel channel);

// Darkening pass. Seeds carry the *previous* level at a cell that just became
// dark (set to 0 by the caller or by this function). Neighbors with a lower
// level are enqueued for further darkening; neighbors with a higher level are
// written into `out_repropagation` as fresh sources that will re-fill the
// region after the dark wave passes.
void removeLight(LightView& view,
                 std::deque<LightNode> seeds,
                 LightChannel channel,
                 std::deque<LightNode>& out_repropagation);

// Convenience: call this from World::setBlock *after* the block has already
// been written. Looks at the delta (emission/opacity) between prev and next
// and schedules the right combination of removal/propagation for both
// channels. The caller resolves emission/opacity from its block database so
// this header stays free of Block.hpp (which transitively pulls in SFML).
void relightOnBlockChange(LightView& view,
                          int wx, int wy,
                          uint8_t prev_emission, uint8_t prev_opacity,
                          uint8_t next_emission, uint8_t next_opacity);

// Fresh chunk: pour sky light from the top down, then let the queue sweep
// sideways. Caller provides chunk dimensions so this header can stay free of
// Chunk.hpp (otherwise the dependency chain pulls in SFML/BlockEntity).
void initializeChunkSkyLight(LightView& view,
                             int chunk_position,
                             int chunk_width,
                             int chunk_height);

#endif // LIGHTING_SYSTEM_HPP
