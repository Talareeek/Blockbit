#include "../include/BlockAtlas.hpp"
#include "../include/AssetManager.hpp"

void BlockAtlas::add(AssetManager::GameTextureID textureID)
{
    textureIDs.push_back(textureID);
}

bool BlockAtlas::build()
{
    if (textureIDs.empty()) return false;

    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;

    for (AssetManager::GameTextureID id : textureIDs)
    {
        const sf::Vector2u size = AssetManager::getGameTexture(id).getSize();

        atlasWidth += size.x;

        if (size.y > atlasHeight) atlasHeight = size.y;
    }

    sf::Image atlasImage({atlasWidth, atlasHeight}, sf::Color::Transparent);

    uint32_t x = 0;

    for (AssetManager::GameTextureID id : textureIDs)
    {
        const sf::Texture& tex = AssetManager::getGameTexture(id);

        const sf::Vector2u size = tex.getSize();

        sf::Image img = tex.copyToImage();

        if (!atlasImage.copy(img, {x, 0}, sf::IntRect({0, 0}, {(int)size.x, (int)size.y})))
            return false;

        uvMap[id] = {{(int)x, 0}, {(int)size.x, (int)size.y}};

        x += size.x;
    }

    if (!atlasTexture.has_value()) atlasTexture.emplace();
    if (!atlasTexture->loadFromImage(atlasImage))
        return false;

    atlasTexture->setSmooth(false);

    return true;
}

const sf::Texture& BlockAtlas::getTexture()
{
    return *atlasTexture;
}

const sf::IntRect& BlockAtlas::getUV(AssetManager::GameTextureID textureID)
{
    return uvMap.at(textureID);
}