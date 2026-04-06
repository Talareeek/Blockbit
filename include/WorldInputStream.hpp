#ifndef WORLD_INPUT_STREAM_HPP
#define WORLD_INPUT_STREAM_HPP

#include <string>
#include <filesystem>
#include "World.hpp"

class World;

class WorldInputStream
{
private:

    std::string filename;

    std::filesystem::path filepath;

public:

    WorldInputStream(const std::string& filename)
    {
        std::string home;

        #ifdef _WIN32

            home = std::getenv("APPDATA");

        #elif __linux__

            home = std::getenv("HOME");

        #endif

        std::filesystem::path path(home);

        if(!std::filesystem::exists(path / "Blockbit"))
        {
            std::filesystem::create_directory(path / "Blockbit");
            path /= "Blockbit";
        }

        if(!std::filesystem::exists(path / "saves"))
        {
            std::filesystem::create_directory(path / "saves");
            path /= "saves";
        }

        path /= filename;
        this->filename = path.string();

        filepath = path;
    }

    WorldInputStream(const std::filesystem::path filepath) : filepath{filepath}{}

    void read(World& world);

    void readChunk(Chunk& chunk);

    void readManifest(World& world);

    void readEntities(World& world);

    void readData(World& world);

};

#endif // WORLD_INPUT_STREAM_HPP