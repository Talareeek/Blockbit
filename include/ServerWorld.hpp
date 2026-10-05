#ifndef SERVER_WORLD_HPP
#define SERVER_WORLD_HPP

#include "World.hpp"

class ServerWorld : public World
{
private:

    std::filesystem::path path;
    std::string name = "world";

    unsigned int seed;

    std::unique_ptr<WorldGenerator> world_generator;
    GenerationProperties generation_properties;
    PerlinNoise perlin{0};

public:

    ServerWorld(const std::filesystem::path path);
    ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties);

    const std::string& getName() const { return name; }
    void setName(const std::string& name) { this->name = name; }

    sf::Vector2<double> getSpawnPoint();
    void generateWorldSpawn();

    void save();
    void load();

    // MANIFEST
    void saveManifest();
    void loadManifest();

    // DATA
    void saveData();
    void loadData();

    // CHUNK
    void saveChunk(int chunk_position);
    void saveChunkEnvironment(int chunk_position);
    void saveChunkEntities(int chunk_position);
    void savePlayer(UUID entity_id);

    void loadChunk(int chunk_position);   
    void loadChunkEnvironment(int chunk_position); 
    void loadChunkEntities(int chunk_position);
    UUID loadPlayer(std::string nickname);
    

    bool hasChunkFile(int chunk_position) const;
    bool playerFileExist(std::string nickname) const;

    void loadOrCreateChunk(int chunk_position);

};

#endif // SERVER_WORLD_HPP