#ifndef FALLING_BLOCK_COMPONENT_HPP
#define FALLING_BLOCK_COMPONENT_HPP

#include "Component.hpp"
#include "Block.hpp"

// Tag component for an entity that represents a block in free-fall (sand,
// gravel, …). The entity itself has Transform + Physics + Render so it falls
// and renders normally; this component just remembers which block to lay back
// down when FallingBlockSystem detects the landing.
//
// Server-only: the client never needs to know "this is a falling sand" vs
// "this is a thrown item" — it just draws whatever RenderComponent says.
struct FallingBlockComponent : public Component
{
    BlockID  block_id = BlockID::Air;
    uint16_t metadata = 0;

    FallingBlockComponent() = default;
    FallingBlockComponent(BlockID id, uint16_t meta) : block_id(id), metadata(meta) {}

    std::string name() const override { return "FallingBlockComponent"; }

    ComponentSide side() const override { return ComponentSide::Server; }

    Tag serialize() const override
    {
        TagCompound c;
        c["block_id"] = Tag(static_cast<uint32_t>(block_id));
        c["metadata"] = Tag(metadata);
        return Tag(c);
    }

    void deserialize(const Tag& tag) override
    {
        block_id = static_cast<BlockID>(tag["block_id"].get<uint32_t>());
        metadata = tag["metadata"].get<uint16_t>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<FallingBlockComponent>(*this);
    }
};

#endif // FALLING_BLOCK_COMPONENT_HPP
