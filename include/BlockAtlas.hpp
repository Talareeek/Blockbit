#ifndef BLOCK_ATLAS_HPP
#define BLOCK_ATLAS_HPP

#include <SFML/Graphics.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

#include "AssetManager.hpp"

class BlockAtlas
{
private:

    inline static std::vector<AssetManager::GameTextureID> textureIDs;

    inline static std::unordered_map<AssetManager::GameTextureID, sf::IntRect> uvMap;

    // optional so construction is deferred to build() — the sf::Texture ctor
    // touches SFML's GL/activity state, which on Android is only valid once
    // ANativeActivity_onCreate has run.
    inline static std::optional<sf::Texture> atlasTexture;

public:

    static void add(AssetManager::GameTextureID textureID);

    static bool build();

    static const sf::Texture& getTexture();

    static const sf::IntRect& getUV(AssetManager::GameTextureID textureID);
};

#endif // BLOCK_ATLAS_HPP