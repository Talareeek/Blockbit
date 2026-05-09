#ifndef BLOCK_ATLAS_HPP
#define BLOCK_ATLAS_HPP

#include <SFML/Graphics.hpp>

#include <unordered_map>
#include <vector>

class BlockAtlas
{
private:

    inline static std::vector<uint32_t> textureIDs;

    inline static std::unordered_map<uint32_t, sf::IntRect> uvMap;

    inline static sf::Texture atlasTexture;

public:

    static void add(uint32_t textureID);

    static bool build();

    static const sf::Texture& getTexture();

    static const sf::IntRect& getUV(uint32_t textureID);
};

#endif // BLOCK_ATLAS_HPP