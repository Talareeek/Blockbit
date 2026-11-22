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
    BlockKey  block_id = Blocks::Air;
    uint16_t metadata = 0;

    FallingBlockComponent() = default;
    FallingBlockComponent(BlockKey id, uint16_t meta) : block_id(id), metadata(meta) {}

    std::string name() const override { return "FallingBlockComponent"; }

    ComponentSide side() const override { return ComponentSide::Server; }

    Tag serializeForStorage() const override
    {
        TagCompound c;
        c["block_id"] = Tag(block_id);
        c["metadata"] = Tag(metadata);
        return Tag(c);
    }

    void deserializeFromStorage(const Tag& tag) override
    {
        if (tag["block_id"].type() == TagType::T_STRING)
            block_id = tag["block_id"].get<std::string>();
        else
            block_id = blockKeyFromLegacySave(tag["block_id"].get<uint32_t>());
        metadata = tag["metadata"].get<uint16_t>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<FallingBlockComponent>(*this);
    }
};

#endif // FALLING_BLOCK_COMPONENT_HPP
