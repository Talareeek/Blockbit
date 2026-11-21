#ifndef RENDER_COMPONENT_HPP
#define RENDER_COMPONENT_HPP

#include "Component.hpp"
#include "AssetManager.hpp"
#include "ContentProfiles.hpp"
#include "GameRegistries.hpp"

#include <cstdint>
#include <SFML/Graphics/Rect.hpp>
#include <string>
#include <sstream>
#include <stdexcept>

struct RenderComponent : public Component
{
    std::string texture_id;
    sf::IntRect uv;
    sf::Vector2f size;

    uint16_t runtime_texture_id;

    // When true the entity sprite is multiplied by the world lightmap at its
    // center cell (same formula as the chunk lighting shader: block_light /
    // sky_light * day scaling). Set false for things that must stay at full
    // brightness regardless of time of day — e.g. lightning bolts, UI-ish
    // entities, glowing orbs.
    bool affected_by_lighting = true;

    RenderComponent(uint16_t runtime_texture_id = 0, sf::IntRect uv = {}, sf::Vector2f size = {}, bool affected_by_lighting = true) : texture_id(gameRegistries().textures.keyOf(runtime_texture_id)), uv(uv), size(size), runtime_texture_id(runtime_texture_id), affected_by_lighting(affected_by_lighting)
    {

    }

    RenderComponent(std::string texture_id, sf::IntRect uv = {}, sf::Vector2f size = {}, bool affected_by_lighting = true) : texture_id(texture_id), uv(uv), size(size), runtime_texture_id(gameRegistries().textures.find(texture_id).value()), affected_by_lighting(affected_by_lighting)
    {

    }

    std::string name() const override
    {
        return "RenderComponent";
    }

    Tag serializeForStorage() const override
    {
        TagCompound compound;

        compound["texture"] = Tag(ContentProfiles::nameFromTexture(runtime_texture_id));

        compound["uv"] = Tag(TagCompound({
            {"x", Tag(uv.position.x)},
            {"y", Tag(uv.position.y)},
            {"w", Tag(uv.size.x)},
            {"h", Tag(uv.size.y)}
        }));

        compound["size"] = Tag(TagCompound({
            {"x", Tag(size.x)},
            {"y", Tag(size.y)}
        }));

        compound["affected_by_lighting"] = Tag(affected_by_lighting);

        return Tag(compound);
    }

    void deserializeFromStorage(const Tag& tag) override
    {
        const std::string key = tag["texture"].get<std::string>();
        auto id = ContentProfiles::textureFromName(key);
        if (!id) throw std::runtime_error("RenderComponent: unknown texture key: " + key);
        runtime_texture_id = *id;

        const auto& uvTag = tag["uv"].get<TagCompound>();

        uv.position.x = uvTag.at("x").get<int>();
        uv.position.y = uvTag.at("y").get<int>();
        uv.size.x = uvTag.at("w").get<int>();
        uv.size.y = uvTag.at("h").get<int>();

        const auto& sizeTag = tag["size"].get<TagCompound>();

        size.x = sizeTag.at("x").get<float>();
        size.y = sizeTag.at("y").get<float>();

        // Backwards-compat: older saves and pre-flag snapshots don't carry
        // this field. Default to lit so existing entities render as before.
        const auto& root = tag.get<TagCompound>();
        auto it = root.find("affected_by_lighting");
        affected_by_lighting = (it != root.end()) ? it->second.get<bool>() : true;
    }

    Tag serializeForTransport() const override
    {
        TagCompound compound;

        compound["runtime_texture"] = Tag(runtime_texture_id);

        compound["uv"] = Tag(TagCompound({
            {"x", Tag(uv.position.x)},
            {"y", Tag(uv.position.y)},
            {"w", Tag(uv.size.x)},
            {"h", Tag(uv.size.y)}
        }));

        compound["size"] = Tag(TagCompound({
            {"x", Tag(size.x)},
            {"y", Tag(size.y)}
        }));

        compound["affected_by_lighting"] = Tag(affected_by_lighting);

        return Tag(compound);
    }

    void deserializeFromTransport(const Tag& tag) override
    {
        runtime_texture_id = tag["runtime_texture"].get<std::uint16_t>();

        const auto& uvTag = tag["uv"].get<TagCompound>();

        uv.position.x = uvTag.at("x").get<int>();
        uv.position.y = uvTag.at("y").get<int>();
        uv.size.x = uvTag.at("w").get<int>();
        uv.size.y = uvTag.at("h").get<int>();

        const auto& sizeTag = tag["size"].get<TagCompound>();

        size.x = sizeTag.at("x").get<float>();
        size.y = sizeTag.at("y").get<float>();

        affected_by_lighting = tag["affected_by_lighting"].get<bool>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<RenderComponent>(*this);
    }
};

#endif // RENDER_COMPONENT_HPP
