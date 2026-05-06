#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <cstdlib>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <unordered_map>

class AssetManager
{
public:

    enum class SoundID : uint32_t
    {
        Click
    };

    enum class MusicID : uint32_t
    {
        Alpha // Talarek - Alpha
    };

//private:

    static std::unordered_map<uint32_t, sf::Texture> textures;
    static std::unordered_map<uint32_t, sf::Font> fonts;
    static std::unordered_map<SoundID, sf::SoundBuffer> sounds;
    static std::unordered_map<MusicID, sf::Music> musics;

public:    

    static void loadTexture(uint32_t id, const std::string& path);
    static void loadFont(uint32_t id, const std::string& path);
    static void loadSound(SoundID id, const std::string& path);
    static void loadMusic(MusicID id, const std::string& path);

    static sf::Texture& getTexture(uint32_t id);
    static sf::Font& getFont(uint32_t id);
    static sf::SoundBuffer& getSound(SoundID id);
    static sf::Music& getMusic(MusicID id);
};

#endif // ASSET_MANAGER_HPP