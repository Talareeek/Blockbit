#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <cstdlib>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <unordered_map>
#include "Registry.hpp"

class AssetManager
{
public:

    enum class UITextureID : uint32_t
    {
        Coin,
        Block_Overlay,
        Inventory,
        Hotbar,
        UIBackground,
        Logo,
        Chest,
        Furnace,
        Crafting,
        Upgrade,
        Border
    };

    enum class FontID : uint32_t
    {
        PressStart2P,
        FiraCode
    };

    enum class ShaderID : uint32_t
    {
        Sun,
        Brightness,
        BlockAnimation,
        LightingOverlay
    };

    enum class SoundID : uint32_t
    {
        Click
    };

    enum class MusicID : uint32_t
    {
        Dream // https://opengameart.org/content/mainmenu-music
    };

//private:

    static std::unordered_map<uint32_t, sf::Texture> textures;
    static std::unordered_map<RuntimeID, sf::Texture> game_textures;
    static std::unordered_map<UITextureID, sf::Texture> ui_textures;
    static std::unordered_map<FontID, sf::Font> fonts;
    static std::unordered_map<ShaderID, sf::Shader> shaders;
    static std::unordered_map<SoundID, sf::SoundBuffer> sounds;
    static std::unordered_map<MusicID, sf::Music> musics;

public:    

    //[[deprecated]] static void loadTexture(uint32_t id, const std::string& path);
    static void loadGameTexture(RuntimeID id, const std::string& path);
    static void loadUITexture(UITextureID id, const std::string& path);
    static void loadFont(FontID id, const std::string& path);
    static void loadShader(ShaderID id, const std::string& path);
    static void loadSound(SoundID id, const std::string& path);
    static void loadMusic(MusicID id, const std::string& path);

    //[[deprecated]] static sf::Texture& getTexture(uint32_t id);
    static sf::Texture& getGameTexture(RuntimeID id);
    static sf::Texture& getUITexture(UITextureID id);
    static sf::Font& getFont(FontID id);
    static sf::Shader& getShader(ShaderID id);
    static sf::SoundBuffer& getSound(SoundID id);
    static sf::Music& getMusic(MusicID id);
};

#endif // ASSET_MANAGER_HPP
