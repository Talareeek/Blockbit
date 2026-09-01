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

    enum class GameTextureID : uint32_t
    {
        // BLOCKS
        Stone,
        Grass,
        Dirt,
        Cobblestone,
        Obsidian,
        Bedrock,
        Water,
        Iron_Ore,
        Gold_Ore,
        Diamond_Ore,
        Ruby_Ore,
        Oak_Log,
        Oak_Leaves,
        Oak_Planks,
        Woodcutter,
        Fire,
        Sand,
        Coarse_Dirt,
        Snow,


        // ITEMS
        Dynamite,
        Bucket,
        Water_Bucket,
        Lighter,
        Ruby,
        Stick,


        // ENTITIES
        Player,
        Pig,

        Wooden_Pickaxe,
        Stone_Pickaxe,
        Iron_Pickaxe,
        Gold_Pickaxe,
        Diamond_Pickaxe,

        Wooden_Axe,
        Stone_Axe,
        Iron_Axe,
        Gold_Axe,
        Diamond_Axe,

        Wooden_Shovel,
        Stone_Shovel,
        Iron_Shovel,
        Gold_Shovel,
        Diamond_Shovel
    };

    enum class UITextureID : uint32_t
    {
        Coin,
        Block_Overlay,
        Inventory,
        Hotbar,
        UIBackground,
        Logo
    };

    enum class FontID : uint32_t
    {
        PressStart2P,
        FiraCode
    };

    enum class ShaderID : uint32_t
    {
        Sun,
        Brightness
    };

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
    static std::unordered_map<GameTextureID, sf::Texture> game_textures;
    static std::unordered_map<UITextureID, sf::Texture> ui_textures;
    static std::unordered_map<FontID, sf::Font> fonts;
    static std::unordered_map<ShaderID, sf::Shader> shaders;
    static std::unordered_map<SoundID, sf::SoundBuffer> sounds;
    static std::unordered_map<MusicID, sf::Music> musics;

public:    

    //[[deprecated]] static void loadTexture(uint32_t id, const std::string& path);
    static void loadGameTexture(GameTextureID id, const std::string& path);
    static void loadUITexture(UITextureID id, const std::string& path);
    static void loadFont(FontID id, const std::string& path);
    static void loadShader(ShaderID id, const std::string& path);
    static void loadSound(SoundID id, const std::string& path);
    static void loadMusic(MusicID id, const std::string& path);

    //[[deprecated]] static sf::Texture& getTexture(uint32_t id);
    static sf::Texture& getGameTexture(GameTextureID id);
    static sf::Texture& getUITexture(UITextureID id);
    static sf::Font& getFont(FontID id);
    static sf::Shader& getShader(ShaderID id);
    static sf::SoundBuffer& getSound(SoundID id);
    static sf::Music& getMusic(MusicID id);
};

#endif // ASSET_MANAGER_HPP