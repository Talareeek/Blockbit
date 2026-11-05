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

        std::uint64_t ar = 0, ag = 0, ab = 0, acount = 0;
        for (std::uint32_t py = 0; py < size.y; ++py)
        {
            for (std::uint32_t px = 0; px < size.x; ++px)
            {
                sf::Color c = img.getPixel({px, py});
                if (c.a == 0) continue;
                ar += c.r;
                ag += c.g;
                ab += c.b;
                ++acount;
            }
        }
        averageColors[id] = acount > 0
            ? sf::Color(static_cast<std::uint8_t>(ar / acount),
                        static_cast<std::uint8_t>(ag / acount),
                        static_cast<std::uint8_t>(ab / acount))
            : sf::Color::Transparent;

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

sf::Color BlockAtlas::getAverageColor(AssetManager::GameTextureID textureID)
{
    auto it = averageColors.find(textureID);
    if (it == averageColors.end()) return sf::Color::Transparent;
    return it->second;
}