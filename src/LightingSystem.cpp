#include "../include/LightingSystem.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace
{
    // Direction order mirrors BlockUpdate's PP/NC order so that if a future
    // feature correlates light propagation with block updates, it does so in
    // a predictable sequence. Changing this order will shift which neighbor
    // "wins" a tie but will not break correctness.
    struct Offset { int dx; int dy; };
    constexpr std::array<Offset, 4> kDirections = {{
        {-1,  0},  // Left
        { 1,  0},  // Right
        { 0, -1},  // Down  (y decreases downward)
        { 0,  1}   // Up
    }};

    constexpr uint8_t kMaxLevel = 15;
    constexpr uint8_t kAirOpacity = 1;

    // Light propagates through both air AND blocks. Air cells pay 1 per step;
    // any non-air cell pays 2 (blocks attenuate twice as fast). No cell fully
    // blocks — a thick wall just drops the level to zero after enough steps.
    uint8_t stepCost(uint8_t target_opacity, LightChannel /*ch*/, int /*dy*/)
    {
        return target_opacity <= kAirOpacity ? 1 : 2;
    }
}

void propagateLight(LightView& view,
                    std::deque<LightNode> seeds,
                    LightChannel channel)
{
    while (!seeds.empty())
    {
        LightNode node = seeds.front();
        seeds.pop_front();

        if (node.level <= 1) continue;

        for (const Offset& off : kDirections)
        {
            int nx = node.wx + off.dx;
            int ny = node.wy + off.dy;
            if (!view.inBounds(nx, ny)) continue;

            uint8_t cost = stepCost(view.getOpacity(nx, ny), channel, off.dy);
            if (cost >= node.level) continue;

            uint8_t new_level = static_cast<uint8_t>(node.level - cost);
            uint8_t current   = view.getLight(nx, ny, channel);
            if (new_level <= current) continue;

            view.setLight(nx, ny, channel, new_level);
            seeds.push_back({nx, ny, new_level});
        }
    }
}

void removeLight(LightView& view,
                 std::deque<LightNode> seeds,
                 LightChannel channel,
                 std::deque<LightNode>& out_repropagation)
{
    // Classic Minecraft "two-queue" dark pass. For each popped cell we look at
    // neighbors: those strictly dimmer than this cell's recorded level are
    // downstream (we were lighting them, so they go dark too). Those at or
    // above our recorded level are independent sources and get recorded as
    // seeds for the re-propagation pass that fills the hole back in.
    while (!seeds.empty())
    {
        LightNode node = seeds.front();
        seeds.pop_front();

        for (const Offset& off : kDirections)
        {
            int nx = node.wx + off.dx;
            int ny = node.wy + off.dy;
            if (!view.inBounds(nx, ny)) continue;

            uint8_t n_level = view.getLight(nx, ny, channel);
            if (n_level == 0) continue;

            if (n_level < node.level)
            {
                view.setLight(nx, ny, channel, 0);
                seeds.push_back({nx, ny, n_level});
            }
            else
            {
                out_repropagation.push_back({nx, ny, n_level});
            }
        }
    }
}

void relightOnBlockChange(LightView& view,
                          int wx, int wy,
                          uint8_t prev_emission, uint8_t prev_opacity,
                          uint8_t next_emission, uint8_t next_opacity)
{
    if (!view.inBounds(wx, wy)) return;

    const bool opacity_up   = next_opacity > prev_opacity;
    const bool opacity_down = next_opacity < prev_opacity;
    const bool emission_up   = next_emission > prev_emission;
    const bool emission_down = next_emission < prev_emission;

    auto handleChannel = [&](LightChannel ch, bool emission_matters)
    {
        std::deque<LightNode> removal;
        std::deque<LightNode> repropagation;

        uint8_t current = view.getLight(wx, wy, ch);

        // The cell itself may be invalid if we got more opaque or if its own
        // emission dropped. The simplest safe move is to clear it and treat
        // its old value as the dark seed; the re-propagation pass will refill.
        const bool self_may_be_stale =
            opacity_up ||
            (emission_matters && emission_down);
        if (self_may_be_stale && current > 0)
        {
            view.setLight(wx, wy, ch, 0);
            removal.push_back({wx, wy, current});
        }

        removeLight(view, std::move(removal), ch, repropagation);

        // A newly-opened or newly-translucent cell lets its neighbors flood
        // in. Seeding every neighbor is correct: propagate skips any step
        // that doesn't actually raise the target.
        if (opacity_down)
        {
            for (const Offset& off : kDirections)
            {
                int nx = wx + off.dx;
                int ny = wy + off.dy;
                if (!view.inBounds(nx, ny)) continue;
                uint8_t n = view.getLight(nx, ny, ch);
                if (n > 1) repropagation.push_back({nx, ny, n});
            }
            // The cell itself may now receive sky light directly from above,
            // or block light bleed from any neighbor. Seed its own slot too.
            uint8_t self_now = view.getLight(wx, wy, ch);
            if (self_now > 1) repropagation.push_back({wx, wy, self_now});
        }

        if (emission_matters && next_emission > 0)
        {
            if (next_emission > view.getLight(wx, wy, ch))
                view.setLight(wx, wy, ch, next_emission);
            repropagation.push_back({wx, wy, next_emission});
        }

        propagateLight(view, std::move(repropagation), ch);
    };

    handleChannel(LightChannel::Block, /*emission_matters=*/true);

    // Sky changes also propagate downward through the column. Opacity-up:
    // the column below (wx, wy) may lose light; the removeLight sweep
    // handles that because setLight(wx, wy, 0) seeds the dark wave which
    // walks down into the column.
    //
    // Opacity-down (a wall became air): the cell directly above may now be
    // able to pour sky light into (wx, wy). Seeding (wx, wy+1)'s current
    // value in the generic opacity_down branch above covers that.
    (void)emission_up;  // sky channel ignores emission
    handleChannel(LightChannel::Sky, /*emission_matters=*/false);
}

void initializeChunkSkyLight(LightView& view,
                             int chunk_position,
                             int chunk_width,
                             int chunk_height)
{
    std::deque<LightNode> sky_seeds;
    std::deque<LightNode> block_seeds;

    const int base_x = chunk_position * chunk_width;

    for (int local_x = 0; local_x < chunk_width; ++local_x)
    {
        const int wx = base_x + local_x;
        bool sky_exposed = true;

        for (int y = chunk_height - 1; y >= 0; --y)
        {
            const uint8_t op = view.getOpacity(wx, y);

            // Any air cell with no block above it gets the full sky level
            // directly — the shader scales it down with the day/night cycle.
            // The first non-air cell seen in the column closes sky exposure
            // for everything below; those cells start at 0 and the BFS fills
            // them in from the exposed cap plus from any lateral neighbors.
            if (sky_exposed && op <= kAirOpacity)
            {
                view.setLight(wx, y, LightChannel::Sky, kMaxLevel);
                sky_seeds.push_back({wx, y, kMaxLevel});
            }
            else
            {
                view.setLight(wx, y, LightChannel::Sky, 0);
                if (op > kAirOpacity) sky_exposed = false;
            }

            // Collect block-light emitters at the same time to avoid a second
            // chunk sweep.
            uint8_t emission = view.getEmission(wx, y);
            if (emission > 0)
            {
                view.setLight(wx, y, LightChannel::Block, emission);
                if (emission > 1) block_seeds.push_back({wx, y, emission});
            }
            else
            {
                view.setLight(wx, y, LightChannel::Block, 0);
            }
        }
    }

    propagateLight(view, std::move(sky_seeds),   LightChannel::Sky);
    propagateLight(view, std::move(block_seeds), LightChannel::Block);
}
