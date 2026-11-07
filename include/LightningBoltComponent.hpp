#ifndef LIGHTNING_BOLT_COMPONENT_HPP
#define LIGHTNING_BOLT_COMPONENT_HPP

#include "Component.hpp"

#include <string>

// A transient visual + marker for a lightning strike. Server creates the
// entity with a short lifetime; client reads the component to draw a
// procedural zigzag and trigger a screen flash. The server keeps the entity
// alive just long enough for at least one snapshot to reach every client.
struct LightningBoltComponent : public Component
{
    // Seconds remaining before the server deletes this entity. Ticked by
    // WeatherSystem on the server; client only reads it (for the zigzag fade).
    float lifetime_remaining = 0.5f;

    // Deterministic seed used by the client to build the zigzag shape. Both
    // the top-to-ground height and the kink offsets derive from this, so every
    // client draws the same bolt for the same strike.
    uint32_t seed = 0;

    // Height of the bolt above its base position, in blocks. Server picks
    // this based on the strike target so the top sits comfortably off-screen.
    float height = 40.0f;

    std::string name() const override
    {
        return "LightningBoltComponent";
    }

    ComponentSide side() const override
    {
        return ComponentSide::Both;
    }

    Tag serialize() const override
    {
        TagCompound compound;
        compound["lifetime_remaining"] = Tag(lifetime_remaining);
        compound["seed"]               = Tag(seed);
        compound["height"]             = Tag(height);
        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        lifetime_remaining = tag["lifetime_remaining"].get<float>();
        seed               = tag["seed"].get<uint32_t>();
        height             = tag["height"].get<float>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<LightningBoltComponent>(*this);
    }
};

#endif // LIGHTNING_BOLT_COMPONENT_HPP
