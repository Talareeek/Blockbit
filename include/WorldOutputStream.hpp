#ifndef WORLD_OUTPUT_STREAM_HPP
#define WORLD_OUTPUT_STREAM_HPP

#include <string>
#include <fstream>
#include <filesystem>
#include <cstdlib>

#include "PhysicsComponent.hpp"
#include "RenderComponent.hpp"

#include "ComponentType.hpp"

class World;

class WorldOutputStream
{
private:

    std::string filename;

    std::filesystem::path filepath;

public:

    WorldOutputStream(const std::string& filename)
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

    WorldOutputStream(const std::filesystem::path filepath) : filepath{filepath}{}

    void write(const World& world);

};

#endif // WORLD_OUTPUT_STREAM_HPP