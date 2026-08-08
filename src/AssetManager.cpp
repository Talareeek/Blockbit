#include "../include/AssetManager.hpp"

std::unordered_map<uint32_t, sf::Texture> AssetManager::textures;
std::unordered_map<AssetManager::FontID, sf::Font> AssetManager::fonts;
std::unordered_map<AssetManager::SoundID, sf::SoundBuffer> AssetManager::sounds;
std::unordered_map<AssetManager::MusicID, sf::Music> AssetManager::musics;

void AssetManager::loadTexture(uint32_t id, const std::string& path)
{
    sf::Texture texture;
    if(!texture.loadFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load texture: " + path);
    
    textures[id] = std::move(texture);
}

void AssetManager::loadFont(AssetManager::FontID id, const std::string& path)
{
    sf::Font font;
    if(!font.openFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load font: " + path);

    fonts[id] = std::move(font);
}

void AssetManager::loadSound(AssetManager::SoundID id, const std::string& path)
{
    sf::SoundBuffer sound;
    if(!sound.loadFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load sound: " + path);

    sounds[id] = std::move(sound);
}

void AssetManager::loadMusic(AssetManager::MusicID id, const std::string& path)
{
    sf::Music music;
    if(!music.openFromFile(path)) throw std::runtime_error(std::string("AssetManager:\n") + "Failed to load music: " + path);

    musics[id] = std::move(music);
}

sf::Texture& AssetManager::getTexture(uint32_t id)
{
    if(textures.find(id) == textures.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Texture not found: " + std::to_string(id));
    return textures.at(id);
}

sf::Font& AssetManager::getFont(AssetManager::FontID id)
{
    if(fonts.find(id) == fonts.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Font not found: " + std::to_string(static_cast<uint32_t>(id)));
    return fonts.at(id);
}

sf::SoundBuffer& AssetManager::getSound(AssetManager::SoundID id)
{
    if(sounds.find(id) == sounds.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Sound not found: " + std::to_string(static_cast<uint32_t>(id)));
    return sounds.at(id);
}

sf::Music& AssetManager::getMusic(AssetManager::MusicID id)
{
    if(musics.find(id) == musics.end()) throw std::runtime_error(std::string("AssetManager:\n") + "Music not found: " + std::to_string(static_cast<uint32_t>(id)));
    return musics.at(id);
}