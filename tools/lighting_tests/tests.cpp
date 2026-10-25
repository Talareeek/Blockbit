// Standalone unit tests for LightingSystem. No SFML, no World — just the
// pure core (LightingSystem.hpp/.cpp) wired against an in-memory 2D grid.
//
// Build: ./build.sh
// Run:   ./lighting_tests
//
// Each check uses plain assert(). On success the binary exits 0 and prints
// a line per scenario; on failure assert() aborts and the shell gets non-zero.

#include "../../include/LightingSystem.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

namespace
{
    // Minimal LightView backed by three flat uint8_t grids (block light, sky
    // light, opacity) and one for emission. World coords are mapped onto the
    // grid as (wx, wy) directly; the grid is placed at origin, so negative
    // coords are treated as out-of-bounds.
    class GridView : public LightView
    {
    public:
        GridView(int w, int h, uint8_t default_opacity)
            : w_{w}, h_{h},
              block_(w * h, 0),
              sky_(w * h, 0),
              opacity_(w * h, default_opacity),
              emission_(w * h, 0)
        {}

        uint8_t getLight(int wx, int wy, LightChannel ch) const override
        {
            if (!inBounds(wx, wy)) return 0;
            return (ch == LightChannel::Block ? block_ : sky_)[idx(wx, wy)];
        }
        void setLight(int wx, int wy, LightChannel ch, uint8_t level) override
        {
            if (!inBounds(wx, wy)) return;
            (ch == LightChannel::Block ? block_ : sky_)[idx(wx, wy)] = level;
        }
        uint8_t getOpacity(int wx, int wy) const override
        {
            if (!inBounds(wx, wy)) return 15;
            return opacity_[idx(wx, wy)];
        }
        uint8_t getEmission(int wx, int wy) const override
        {
            if (!inBounds(wx, wy)) return 0;
            return emission_[idx(wx, wy)];
        }
        bool inBounds(int wx, int wy) const override
        {
            return wx >= 0 && wx < w_ && wy >= 0 && wy < h_;
        }

        // Test helpers.
        void setOpacity(int wx, int wy, uint8_t op)
        {
            if (inBounds(wx, wy)) opacity_[idx(wx, wy)] = op;
        }
        void setEmission(int wx, int wy, uint8_t em)
        {
            if (inBounds(wx, wy)) emission_[idx(wx, wy)] = em;
        }
        int width()  const { return w_; }
        int height() const { return h_; }

    private:
        size_t idx(int wx, int wy) const
        {
            return static_cast<size_t>(wy) * static_cast<size_t>(w_) + static_cast<size_t>(wx);
        }

        int w_, h_;
        std::vector<uint8_t> block_;
        std::vector<uint8_t> sky_;
        std::vector<uint8_t> opacity_;
        std::vector<uint8_t> emission_;
    };

    // Shorthand: seed a single source at (sx, sy) with level 15, then run propagate.
    void floodSingleSource(GridView& v, int sx, int sy, LightChannel ch, uint8_t level = 15)
    {
        v.setLight(sx, sy, ch, level);
        std::deque<LightNode> seeds;
        seeds.push_back({sx, sy, level});
        propagateLight(v, std::move(seeds), ch);
    }

    void test_pointSource_diamond()
    {
        // Open air grid, 15x15 opacity-1. Source at center level 15.
        // Manhattan distance from the source should equal (15 - value).
        GridView v(15, 15, /*default_opacity=*/1);
        floodSingleSource(v, 7, 7, LightChannel::Block, 15);

        for (int y = 0; y < v.height(); ++y)
        {
            for (int x = 0; x < v.width(); ++x)
            {
                int d = std::abs(x - 7) + std::abs(y - 7);
                int expected = (d >= 15) ? 0 : (15 - d);
                int actual = v.getLight(x, y, LightChannel::Block);
                assert(actual == expected);
            }
        }
        std::printf("ok: point source produces diamond falloff\n");
    }

    void test_wall_attenuates_double()
    {
        // Air grid, source at (0, 0). Column x=4 is a solid block (opacity 15).
        // Blocks no longer fully block — they just pay cost 2. Pre-wall cells
        // still lose 1 per step; stepping into x=4 pays 2 (block). After the
        // wall, propagation resumes at cost 1 through air.
        //   x=0 → 15, x=1 → 14, x=2 → 13, x=3 → 12,
        //   x=4 (block) → 10, x=5 → 9, x=6 → 8, x=7 → 7, x=8 → 6, x=9 → 5.
        GridView v(10, 1, 1);
        for (int y = 0; y < v.height(); ++y) v.setOpacity(4, y, 15);

        floodSingleSource(v, 0, 0, LightChannel::Block, 15);

        const int expected[10] = {15, 14, 13, 12, 10, 9, 8, 7, 6, 5};
        for (int x = 0; x < v.width(); ++x)
            assert(v.getLight(x, 0, LightChannel::Block) == expected[x]);
        std::printf("ok: solid block attenuates light at cost 2 per step\n");
    }

    void test_block_cost_is_binary()
    {
        // All non-air opacities behave the same (cost = 2). An opacity-3 cell
        // at x=1 and an opacity-15 cell at x=3 both pay 2.
        //   x=0 → 15, x=1 (op 3)  → 13, x=2 (air) → 12,
        //   x=3 (op 15) → 10, x=4 (air) → 9.
        GridView v(10, 1, 1);
        v.setOpacity(1, 0, 3);
        v.setOpacity(3, 0, 15);

        floodSingleSource(v, 0, 0, LightChannel::Block, 15);

        assert(v.getLight(0, 0, LightChannel::Block) == 15);
        assert(v.getLight(1, 0, LightChannel::Block) == 13);
        assert(v.getLight(2, 0, LightChannel::Block) == 12);
        assert(v.getLight(3, 0, LightChannel::Block) == 10);
        assert(v.getLight(4, 0, LightChannel::Block) == 9);
        std::printf("ok: every non-air cell pays the same block cost (2)\n");
    }

    void test_removeSingleSource_fullDecay()
    {
        // Flood from (0,0), then remove the source. Everything must go to 0.
        GridView v(10, 1, 1);
        v.setEmission(0, 0, 15);
        floodSingleSource(v, 0, 0, LightChannel::Block, 15);

        // Remove the source: zero the emitter and seed removeLight.
        uint8_t old = v.getLight(0, 0, LightChannel::Block);
        v.setLight(0, 0, LightChannel::Block, 0);
        v.setEmission(0, 0, 0);
        std::deque<LightNode> dark;  dark.push_back({0, 0, old});
        std::deque<LightNode> repro;
        removeLight(v, std::move(dark), LightChannel::Block, repro);
        propagateLight(v, std::move(repro), LightChannel::Block);

        for (int x = 0; x < v.width(); ++x)
            assert(v.getLight(x, 0, LightChannel::Block) == 0);
        std::printf("ok: removing single source darkens everywhere\n");
    }

    void test_removeOneOfTwoSources_rePropagation()
    {
        // Two sources at (0,0) and (10,0) in a 15-wide row, both level 15.
        // Remove the one at (0,0). Cells near (10,0) must still be lit by
        // the re-propagation pass.
        GridView v(15, 1, 1);
        v.setEmission(0, 0, 15);
        v.setEmission(10, 0, 15);
        floodSingleSource(v, 0, 0,  LightChannel::Block, 15);
        floodSingleSource(v, 10, 0, LightChannel::Block, 15);

        // Both sources active — (5, 0) sees max(15 - 5, 15 - 5) = 10.
        assert(v.getLight(5, 0, LightChannel::Block) == 10);

        // Kill the left source.
        uint8_t old = v.getLight(0, 0, LightChannel::Block);
        v.setLight(0, 0, LightChannel::Block, 0);
        v.setEmission(0, 0, 0);
        std::deque<LightNode> dark;  dark.push_back({0, 0, old});
        std::deque<LightNode> repro;
        removeLight(v, std::move(dark), LightChannel::Block, repro);
        propagateLight(v, std::move(repro), LightChannel::Block);

        // Only right source remains → light is 15 - |x - 10|.
        for (int x = 0; x < v.width(); ++x)
        {
            int expected = std::max(0, 15 - std::abs(x - 10));
            int actual = v.getLight(x, 0, LightChannel::Block);
            assert(actual == expected);
        }
        std::printf("ok: re-propagation fills from the surviving source\n");
    }

    void test_skyInit_openColumn()
    {
        // Fully open column (opacity 1 everywhere). Every cell has no block
        // above it, so every cell is directly seeded at level 15.
        GridView v(1, 10, 1);
        initializeChunkSkyLight(v, /*chunk_position=*/0, /*chunk_width=*/1, /*chunk_height=*/10);
        for (int y = 0; y < 10; ++y)
            assert(v.getLight(0, y, LightChannel::Sky) == 15);
        std::printf("ok: sky light fills open column to 15 top-to-bottom\n");
    }

    void test_skyInit_cappedColumn()
    {
        // Block at y=5. Air cells above (y=6..9) have no block above them,
        // so they're seeded at 15. The block at y=5 and air below start at
        // 0, but BFS leaks sky down through the block at cost 2 per step:
        //   y=5 (block) ← from y=6 (level 15, cost 2) → 13
        //   y=4 (air)   ← from y=5 (13, cost 1) → 12
        //   y=3 (air)   → 11, y=2 → 10, y=1 → 9, y=0 → 8.
        GridView v(1, 10, 1);
        v.setOpacity(0, 5, 15);
        initializeChunkSkyLight(v, 0, 1, 10);

        for (int y = 6; y <= 9; ++y) assert(v.getLight(0, y, LightChannel::Sky) == 15);
        assert(v.getLight(0, 5, LightChannel::Sky) == 13);
        assert(v.getLight(0, 4, LightChannel::Sky) == 12);
        assert(v.getLight(0, 3, LightChannel::Sky) == 11);
        assert(v.getLight(0, 2, LightChannel::Sky) == 10);
        assert(v.getLight(0, 1, LightChannel::Sky) ==  9);
        assert(v.getLight(0, 0, LightChannel::Sky) ==  8);
        std::printf("ok: sky leaks through a solid cap at block cost (2)\n");
    }

    void test_skyInit_translucentColumn()
    {
        // Column of opacity-3 cells at y=5..7. In the new model non-air is
        // non-air — all three pay the same block cost (2) each step.
        //   y=9 → 15 (open air seed), y=8 → 15 (seed, nothing above),
        //   y=7 (block) ← from y=8 (cost 2) → 13,
        //   y=6 (block) → 11, y=5 (block) → 9,
        //   y=4 (air)   ← from y=5 (cost 1) → 8, y=3 → 7, …
        GridView v(1, 10, 1);
        for (int y = 5; y <= 7; ++y) v.setOpacity(0, y, 3);
        initializeChunkSkyLight(v, 0, 1, 10);

        assert(v.getLight(0, 9, LightChannel::Sky) == 15);
        assert(v.getLight(0, 8, LightChannel::Sky) == 15);
        assert(v.getLight(0, 7, LightChannel::Sky) == 13);
        assert(v.getLight(0, 6, LightChannel::Sky) == 11);
        assert(v.getLight(0, 5, LightChannel::Sky) ==  9);
        assert(v.getLight(0, 4, LightChannel::Sky) ==  8);
        std::printf("ok: translucent column pays the same block cost as solid\n");
    }
}

int main()
{
    test_pointSource_diamond();
    test_wall_attenuates_double();
    test_block_cost_is_binary();
    test_removeSingleSource_fullDecay();
    test_removeOneOfTwoSources_rePropagation();
    test_skyInit_openColumn();
    test_skyInit_cappedColumn();
    test_skyInit_translucentColumn();
    std::printf("All lighting tests passed.\n");
    return 0;
}
