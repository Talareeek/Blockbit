#include "../include/WorldInputStream.hpp"
#include "../include/World.hpp"

#include <string>
#include <fstream>
#include <sstream>

void WorldInputStream::read(World& world)
{
    std::ifstream file_stream(file_path, std::ios::binary);

    std::string file;
}

/*
#include "../include/WorldOutputStream.hpp"

#include "../include/World.hpp"

void WorldOutputStream::write(const World& world)
{
    std::string output;

    output += "Blockbit World File\n";
    output += "Version: " + std::to_string(world.getVersion()) + "\n";

    output += '\n';

    output += "Chunks:\n";

    for(auto& [position, chunk] : world.chunks)
    {
        output += "Chunk: " + std::to_string(position) + "\n";
        output += "Blocks:\n";
        
        for(int y = 0; y < CHUNK_HEIGHT; y++)
        {
            for(int x = 0; x < CHUNK_WIDTH; x++)
            {
                output += std::to_string(static_cast<int>(chunk.blocks[y][x])) + ' ';
            }
            output += '\n';
        }
        output += '\n';
    }


    // ENTITIES
    output += "Entities:\n";

    for(const auto& entity : world.entities)
    {
        output += "Entity ID: " + std::to_string(entity.getID()) + "\n";

        for(const auto& [type, component] : entity.getComponents())
        {
            output += "Component Type: ";

            if(type == typeid(PhysicsComponent))
            {
                output += std::to_string(static_cast<unsigned int>(ComponentType::Physics)) + '\n';
                output += std::any_cast<PhysicsComponent>(component).serialize();
            }
            else if(type == typeid(RenderComponent))
            {
                output += std::to_string(static_cast<unsigned int>(ComponentType::Render)) + '\n';
                output += std::any_cast<RenderComponent>(component).serialize();
            }
        }

        output += '\n';
    }

    output += '\n';

    output += "Footer\n";

    std::ofstream file(filename, std::ios::binary);
    if(!file) throw std::runtime_error("Failed to open file for writing: " + filename);

    file << output;
}
*/