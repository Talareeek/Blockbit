#ifndef BLOCK_ATLAS_HPP
#define BLOCK_ATLAS_HPP

#include <SFML/Graphics.hpp>

#include <unordered_map>
#include <vector>

#include "AssetManager.hpp"

class BlockAtlas
{
private:

    inline static std::vector<AssetManager::GameTextureID> textureIDs;

    inline static std::unordered_map<AssetManager::GameTextureID, sf::IntRect> uvMap;

    inline static sf::Texture atlasTexture;

public:

    static void add(AssetManager::GameTextureID textureID);

    static bool build();

    static const sf::Texture& getTexture();

    static const sf::IntRect& getUV(AssetManager::GameTextureID textureID);
};

#endif // BLOCK_ATLAS_HPP