#include "../include/AssetManager.hpp"

std::unordered_map<uint32_t, sf::Texture> AssetManager::textures;
std::unordered_map<uint32_t, sf::Font> AssetManager::fonts;
std::unordered_map<AssetManager::SoundID, sf::SoundBuffer> AssetManager::sounds;

void AssetManager::loadTexture(uint32_t id, const std::string& path)
{
    sf::Texture texture;
    if(!texture.loadFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load texture: " + path);
    
    textures[id] = std::move(texture);
}

void AssetManager::loadFont(uint32_t id, const std::string& path)
{
    sf::Font font;
    if(!font.openFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load font: " + path);

    fonts[id] = std::move(font);
}

void AssetManager::loadSound(SoundID id, const std::string& path)
{
    sf::SoundBuffer sound;
    if(!sound.loadFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load sound: " + path);

    sounds[id] = std::move(sound);
}

sf::Texture& AssetManager::getTexture(uint32_t id)
{
    if(textures.find(id) == textures.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Texture not found: " + std::to_string(id));
    return textures.at(id);
}

sf::Font& AssetManager::getFont(uint32_t id)
{
    if(fonts.find(id) == fonts.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Font not found: " + std::to_string(id));
    return fonts.at(id);
}

sf::SoundBuffer& AssetManager::getSound(SoundID id)
{
    if(sounds.find(id) == sounds.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Sound not found: " + std::to_string(static_cast<uint32_t>(id)));
    return sounds.at(id);
}