#include "../include/BlockAtlas.hpp"
#include "../include/AssetManager.hpp"

void BlockAtlas::add(AssetManager::GameTextureID textureID)
{
    textureIDs.push_back(textureID);
}

bool BlockAtlas::build()
{
    if (textureIDs.empty()) return false;

    const sf::Texture& firstTexture = AssetManager::getGameTexture(textureIDs[0]);

    const uint32_t tileSize = firstTexture.getSize().x;

    const uint32_t atlasWidth = tileSize * textureIDs.size();

    sf::Image atlasImage({atlasWidth, tileSize}, sf::Color::Transparent);

    uint32_t x = 0;

    for (AssetManager::GameTextureID id : textureIDs)
    {
        const sf::Texture& tex = AssetManager::getGameTexture(id);

        sf::Image img = tex.copyToImage();

        if (!atlasImage.copy(img, {x, 0}, sf::IntRect({0, 0}, {(int)tileSize, (int)tileSize})))
            return false;

        uvMap[id] = {{(int)x, 0}, {(int)tileSize, (int)tileSize}};

        x += tileSize;
    }

    if (!atlasTexture.loadFromImage(atlasImage))
        return false;

    atlasTexture.setSmooth(false);

    return true;
}

const sf::Texture& BlockAtlas::getTexture()
{
    return atlasTexture;
}

const sf::IntRect& BlockAtlas::getUV(AssetManager::GameTextureID textureID)
{
    return uvMap.at(textureID);
}