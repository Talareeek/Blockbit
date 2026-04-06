#include "../include/WorldOutputStream.hpp"

#include "../include/World.hpp"

#include "../include/AnimationComponent.hpp"

#include "../include/ItemComponent.hpp"

#include "../include/HealthComponent.hpp"

#include "../include/PreserveComponent.hpp"

#include "../include/ExplosiveComponent.hpp"

#include "../include/TransformComponent.hpp"

#include "../include/InventoryComponent.hpp"

void WorldOutputStream::write(const World& world)
{
    writeManifest(world);
    
    for(const auto& [chunk_position, chunk] : world.chunks)
    {
        writeChunk(chunk);
    }

    writeEntities(world);

    writeData(world);

}


void WorldOutputStream::writeChunk(const Chunk& chunk)
{
    std::filesystem::path path = filepath / ("chunk_" + std::to_string(chunk.chunk_position));

    std::ofstream file(path, std::ios::binary | std::ios::out);

    if(!file) throw std::runtime_error("Failed to open file for writing: " + path.string());


    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            file.write(reinterpret_cast<const char*>(&chunk.blocks[y][x].id), sizeof(BlockID));
            file.write(reinterpret_cast<const char*>(&chunk.blocks[y][x].metadata), sizeof(uint8_t)); 
        }
    }

    file.close();
}

void WorldOutputStream::writeManifest(const World& world)
{
    std::filesystem::path path = filepath / "manifest";

    std::ofstream file(path, std::ios::out);

    if(!file) throw std::runtime_error("Failed to open file for writing: " + path.string());

    file << world.getName() << '\n';
    file << world.getSeed() << '\n';
    file << world.getVersion() << '\n';

    file.close();
}

void WorldOutputStream::writeEntities(const World& world)
{
    std::filesystem::path path = filepath / "entities";

    std::ofstream file(path, std::ios::out);

    if(!file) throw std::runtime_error("Failed to open file for writing: " + path.string());


    for(const auto& entity : world.getEntities())
    {
        file << "Entity ID: " << entity.getID() << '\n';

        for(const auto& [type, component] : entity.getComponents())
        {
            file << "Component Type: ";

            if(type == typeid(PhysicsComponent))
            {
                file << "Physics" << '\n';
                file << std::any_cast<PhysicsComponent>(component).serialize();
            }
            else if(type == typeid(RenderComponent))
            {
                file << "Render" << '\n';
                file << std::any_cast<RenderComponent>(component).serialize();
            }
            else if(type == typeid(AnimationComponent))
            {
                file << "Animation" << '\n';
                file << std::any_cast<AnimationComponent>(component).serialize();
            }
            else if(type == typeid(InventoryComponent))
            {
                file << "Inventory" << '\n';
                file << std::any_cast<InventoryComponent>(component).serialize();
            }
            else if(type == typeid(HealthComponent))
            {
                file << "Health" << '\n';
                file << std::any_cast<HealthComponent>(component).serialize();
            }
            else if(type == typeid(ItemComponent))
            {
                file << "Item" << '\n';
                file << std::any_cast<ItemComponent>(component).serialize();
            }
            else if(type == typeid(PreserveComponent))
            {
                file << "Preserve" << '\n';
                file << (std::any_cast<PreserveComponent>(component) == PreserveComponent::Preserve ? "Preserve" : "Destroy") << '\n';
            }
            else if(type == typeid(ExplosiveComponent))
            {
                file << "Explosive" << '\n';
                file << std::any_cast<ExplosiveComponent>(component).serialize();
            }
            else if(type == typeid(TransformComponent))
            {
                file << "Transform" << '\n';
                file << std::any_cast<TransformComponent>(component).serialize();
            }
        }

        file << '\n';
    }
}


void WorldOutputStream::writeData(const World& world)
{
    std::filesystem::path path = filepath / "data";

    std::ofstream file(path, std::ios::out);

    file << "DayTime: " << world.getDayTime() << '\n';
    file << "Days: " << world.days << '\n';
    file << "Spawn Point: " << world.getSpawnPoint().x << ' ' << world.getSpawnPoint().y << '\n';
    file << "Player ID: " << world.getPlayerID() << '\n';
}