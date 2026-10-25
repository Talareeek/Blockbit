#ifndef WORLD_LIGHT_VIEW_HPP
#define WORLD_LIGHT_VIEW_HPP

#include "LightingSystem.hpp"

class World;

// LightView bound to an existing World. Server and client use this identical
// adapter so the BFS runs over identical state and produces identical results
// — the lightmap never needs to travel over the wire.
//
// Lightmap vectors are allocated lazily on first write (chunks that never see
// a lighting pass stay at 0 bytes).
class WorldLightView : public LightView
{
public:
    explicit WorldLightView(World& world) : world_{world} {}

    uint8_t getLight(int wx, int wy, LightChannel ch) const override;
    void    setLight(int wx, int wy, LightChannel ch, uint8_t level) override;
    uint8_t getOpacity(int wx, int wy) const override;
    uint8_t getEmission(int wx, int wy) const override;
    bool    inBounds(int wx, int wy) const override;

private:
    World& world_;
};

#endif // WORLD_LIGHT_VIEW_HPP
