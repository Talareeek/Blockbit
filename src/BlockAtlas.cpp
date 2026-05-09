#include "../include/BlockAtlas.hpp"
#include "../include/AssetManager.hpp"

void BlockAtlas::add(uint32_t textureID)
{
    textureIDs.push_back(textureID);
}

bool BlockAtlas::build()
{
    if (textureIDs.empty()) return false;

    const sf::Texture& firstTexture = AssetManager::getTexture(textureIDs[0]);

    const uint32_t tileSize = firstTexture.getSize().x;

    const uint32_t atlasWidth = tileSize * textureIDs.size();

    sf::Image atlasImage({atlasWidth, tileSize}, sf::Color::Transparent);

    uint32_t x = 0;

    for (uint32_t id : textureIDs)
    {
        const sf::Texture& tex = AssetManager::getTexture(id);

        sf::Image img = tex.copyToImage();

        atlasImage.copy(img, {x, 0});

        uvMap[id] = {{(int)x, 0}, {(int)tileSize, (int)tileSize}};

        x += tileSize;
    }

    atlasTexture.loadFromImage(atlasImage);

    atlasTexture.setSmooth(false);

    return true;
}

const sf::Texture& BlockAtlas::getTexture()
{
    return atlasTexture;
}

const sf::IntRect& BlockAtlas::getUV(uint32_t textureID)
{
    return uvMap.at(textureID);
}