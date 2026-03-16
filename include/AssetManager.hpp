#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <cstdlib>
#include <SFML/Graphics.hpp>
#include <unordered_map>

class AssetManager
{
private:

    static std::unordered_map<uint32_t, sf::Texture> textures;
    static std::unordered_map<uint32_t, sf::Font> fonts;

public:

    static void loadTexture(uint32_t id, const std::string& path);
    static void loadFont(uint32_t id, const std::string& path);
    static sf::Texture& getTexture(uint32_t id);
    static sf::Font& getFont(uint32_t id);

};

#endif // ASSET_MANAGER_HPP