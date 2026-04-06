#include "../include/WorldInputStream.hpp"
#include "../include/World.hpp"
#include "../include/Chunk.hpp"

#include "../include/TransformComponent.hpp"
#include "../include/InventoryComponent.hpp"
#include "../include/HealthComponent.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/ItemComponent.hpp"
#include "../include/PreserveComponent.hpp"
#include "../include/ExplosiveComponent.hpp"


#include <string>
#include <fstream>
#include <sstream>

void WorldInputStream::read(World& world)
{
    
}

void WorldInputStream::readChunk(Chunk& chunk)
{
    std::filesystem::path path = filepath / ("chunk_" + std::to_string(chunk.chunk_position));

    std::ifstream file(path, std::ios::binary);

    if (!file.is_open()) throw std::runtime_error("Failed to open chunk file: " + path.string());

    for (int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for (int x = 0; x < CHUNK_WIDTH; x++)
        {
            uint32_t blockID;
            uint8_t metadata;
            file.read(reinterpret_cast<char*>(&blockID), sizeof(blockID));
            file.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));
            chunk.blocks[y][x] = Block{static_cast<BlockID>(blockID), metadata};
        }
    }
}

void WorldInputStream::readManifest(World& world)
{
    std::filesystem::path path = filepath / "manifest";

    std::ifstream file(path);

    if(!file) throw std::runtime_error("Failed to open file for reading: " + path.string());

    std::string line;
    while(std::getline(file, line))
    {
        if(line.substr(0, 5) == "Seed:")
        {
            world.setSeed(std::stoll(line.substr(6)));
        }
        else if(line.substr(0, 8) == "Version:")
        {
            //world.setVersion(line.substr(9));
        }
        else if(line.substr(0, 5) == "Name:")
        {
            world.setName(line.substr(6));
        }
    }

    file.close();
}

void WorldInputStream::readEntities(World& world)
{
    std::filesystem::path path = filepath / "entities";

    std::ifstream file(path, std::ios::in);

    if(!file) throw std::runtime_error("Failed to open file for writing: " + path.string());


    std::string line;
    while(std::getline(file, line))
    {
        if(line.substr(0, 10) == "Entity ID:")
        {
            uint32_t entityID = std::stoll(line.substr(11));
            Entity entity(entityID);

            while(std::getline(file, line) && !line.empty())
            {
                if(line.substr(0, 15) == "Component Type:")
                {
                    std::string componentType = line.substr(16);

                    std::string componentData;
                    std::string componentLine;

                    while(std::getline(file, componentLine) && !componentLine.empty() && componentLine.substr(0, 15) != "Component Type:")
                    {
                        componentData += componentLine + '\n';
                    }

                    if(componentType == "Physics")
                    {
                        PhysicsComponent physics;
                        physics.deserialize(componentData);
                        entity.addComponent<PhysicsComponent>(physics);
                    }
                    else if(componentType == "Render")
                    {
                        RenderComponent render;
                        render.deserialize(componentData);
                        entity.addComponent<RenderComponent>(render);
                    }
                    else if(componentType == "Animation")
                    {
                        AnimationComponent animation;
                        animation.deserialize(componentData);
                        entity.addComponent<AnimationComponent>(animation);
                    }
                    else if(componentType == "Inventory")
                    {
                        InventoryComponent inventory(1);
                        inventory.deserialize(componentData);
                        entity.addComponent<InventoryComponent>(inventory);
                    }
                    else if(componentType == "Health")
                    {
                        HealthComponent health;
                        health.deserialize(componentData);
                        entity.addComponent<HealthComponent>(health);
                    }
                    else if(componentType == "Item")
                    {
                        ItemComponent item;
                        item.deserialize(componentData);
                        entity.addComponent<ItemComponent>(item);
                    }
                    else if(componentType == "Preserve")
                    {
                        PreserveComponent preserve;
                        std::string preserveStr;
                        std::getline(file, preserveStr);
                        preserve = (preserveStr == "Preserve" ? PreserveComponent::Preserve : PreserveComponent::Destroy);
                        entity.addComponent<PreserveComponent>(preserve);
                    }
                    else if(componentType == "Explosive")
                    {
                        ExplosiveComponent explosive;
                        explosive.deserialize(componentData);
                        entity.addComponent<ExplosiveComponent>(explosive);
                    }
                    else if(componentType == "Transform")
                    {
                        TransformComponent transform;
                        transform.deserialize(componentData);
                        entity.addComponent<TransformComponent>(transform);
                    }
                }
            }

            world.getEntities().push_back(entity);
        }
    }
}


void WorldInputStream::readData(World& world)
{
    std::filesystem::path path = filepath / "data";

    std::ifstream file(path);

    if(!file) throw std::runtime_error("Failed to open file for reading: " + path.string());

    
}

/*

void WorldOutputStream::writeData(const World& world)
{
    std::filesystem::path path = filepath / "data";

    std::ofstream file(path, std::ios::out);

    file << "DayTime: " << world.getDayTime() << '\n';
    file << "Days: " << world.days << '\n';
    file << "Spawn Point: " << world.getSpawnPoint().x << ' ' << world.getSpawnPoint().y << '\n';
    file << "Player ID: " << world.getPlayerID() << '\n';
}

*/