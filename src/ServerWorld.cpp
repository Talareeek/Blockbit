#include "../include/ServerWorld.hpp"
#include "../include/NormalWorldGenerator.hpp"
#include "../include/PlayerControlledComponent.hpp"

#include <fstream>
#include <zstd.h>
#include <iostream>

ServerWorld::ServerWorld(const std::filesystem::path path) : path{path}
{
    load();

    if(!world_generator) throw("World generator is nullptr");
}

ServerWorld::ServerWorld(const std::string name, const std::filesystem::path path, unsigned int seed, GenerationProperties generation_properties) : name(name), path(path), seed(seed), perlin(seed), generation_properties{generation_properties}
{
    NormalWorldGeneratorProperties properties
    {
        .base_height = generation_properties.base_height,
        .height_scale = generation_properties.height_scale,
        .frequency = generation_properties.frequency,
        .amplitude = generation_properties.amplitude,
        .persistence = generation_properties.persistence
    };

    world_generator = std::make_unique<NormalWorldGenerator>(seed, properties);

    if(!world_generator) throw("World generator is nullptr");

    generateWorldSpawn();
    save();
}

void ServerWorld::save()
{
    saveManifest();
    saveData();

    for(auto& [chunk_position, chunk] : chunks)
    {
        if(chunk.dirty)
        {
            saveChunk(chunk_position);
        }
    }
}

void ServerWorld::load()
{
    if(!std::filesystem::exists(path)) throw std::runtime_error("World path does not exist");

    loadManifest();
    loadData();

    perlin = PerlinNoise(seed);
}


void ServerWorld::saveManifest()
{
    BBT root("manifest");

    root["name"] = Tag(name);


    std::ofstream file(path / "manifest", std::ios::binary);

    if(!file) throw std::runtime_error("Cannot open manifest file");


    std::vector<uint8_t> buffer = root.save();

    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());


    file.close();
}

void ServerWorld::loadManifest()
{
    std::ifstream stream(path / "manifest", std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open manifest file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    name = root["name"].get<std::string>();
}


void ServerWorld::saveData()
{
    BBT data("data");

    data["generation_properties"] = TagCompound();
    data["generation_properties"]["flat"] = Tag(generation_properties.flat);
    data["generation_properties"]["base_height"] = Tag(generation_properties.base_height);
    data["generation_properties"]["height_scale"] = Tag(generation_properties.height_scale);
    data["generation_properties"]["frequency"] = Tag(generation_properties.frequency);
    data["generation_properties"]["amplitude"] = Tag(generation_properties.amplitude);
    data["generation_properties"]["persistence"] = Tag(generation_properties.persistence);

    data["seed"] = Tag(seed);

    data["day_time"] = Tag(getDayTime());
    data["days"] = Tag(days);

    auto buffer = data.save();

    std::ofstream file(path / "data", std::ios::out | std::ios::binary);
    
    file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}

void ServerWorld::loadData()
{
    std::ifstream file(path / "data", std::ios::ate | std::ios::binary);

    if(!file) throw std::runtime_error("Failed to open data file");

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    file.read(reinterpret_cast<char*>(buffer.data()), size);

    file.close();


    BBT data = BBT::load(buffer);

    seed = data["seed"].get<unsigned int>();

    dayTime = data["day_time"].get<float>();
    days = data["days"].get<uint64_t>();
    
    generation_properties =
    {
        .flat = data["generation_properties"]["flat"].get<bool>(),
        .base_height = data["generation_properties"]["base_height"].get<float>(),
        .height_scale = data["generation_properties"]["height_scale"].get<float>(),
        .frequency = data["generation_properties"]["frequency"].get<float>(),
        .amplitude = data["generation_properties"]["amplitude"].get<float>(),
        .persistence = data["generation_properties"]["persistence"].get<float>()
    };

    NormalWorldGeneratorProperties properties
    {
        .base_height = generation_properties.base_height,
        .height_scale = generation_properties.height_scale,
        .frequency = generation_properties.frequency,
        .amplitude = generation_properties.amplitude,
        .persistence = generation_properties.persistence
    };

    world_generator = std::make_unique<NormalWorldGenerator>(seed, properties);
}


void ServerWorld::saveChunk(int chunk_position)
{
    saveChunkEnvironment(chunk_position);
    saveChunkEntities(chunk_position);
}

static constexpr uint32_t CHUNK_FILE_MAGIC = 0x4B434242u;

void ServerWorld::saveChunkEnvironment(int chunk_position)
{
    auto iterator = chunks.find(chunk_position);
    if(iterator == chunks.end()) return;

    size_t block_size = sizeof(BlockID) + sizeof(uint8_t);
    size_t raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * block_size + 16 * sizeof(Climate) + 16 * sizeof(Biome);

    std::vector<char> raw(raw_size);
    char* ptr = raw.data();

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            const Block& block = iterator->second.blocks[y][x];

            std::memcpy(ptr, &block.id, sizeof(BlockID));
            ptr += sizeof(BlockID);

            std::memcpy(ptr, &block.metadata, sizeof(uint8_t));
            ptr += sizeof(uint8_t);
        }
    }

    for(int x = 0; x < 16; x++)
    {
        const Climate& climate = iterator->second.climates[x]; 
        const Biome& biome = iterator->second.biomes[x];

        std::memcpy(ptr, &climate, sizeof(Climate));
        ptr += sizeof(Climate);

        std::memcpy(ptr, &biome, sizeof(Biome));
        ptr += sizeof(Biome);
    }

    size_t bound = ZSTD_compressBound(raw_size);
    std::vector<char> compressed(bound);

    size_t compressed_size = ZSTD_compress(compressed.data(), bound, raw.data(), raw_size, 12);

    if(ZSTD_isError(compressed_size))
    {
        std::cerr << "ZSTD_compress failed for chunk " << chunk_position << ": " << ZSTD_getErrorName(compressed_size) << '\n';
        return;
    }

    std::filesystem::path chunk_dir = path / ("chunk_" + std::to_string(chunk_position));

    std::error_code ec;
    std::filesystem::create_directories(chunk_dir, ec);

    if(ec)
    {
        std::cerr << "Failed to create chunk directory: "
                << chunk_dir << " : "
                << ec.message() << '\n';
        return;
    }

    std::filesystem::path final_path = path / ("chunk_" + std::to_string(chunk_position)) / "environment";
    std::filesystem::path tmp_path = path / ("chunk_" + std::to_string(chunk_position)) / ("environment.tmp");

    {
        std::ofstream file(tmp_path, std::ios::binary | std::ios::trunc);
        if(!file)
        {
            std::cerr << "Failed to open temp file for chunk " << chunk_position << '\n';
            return;
        }

        uint32_t magic = CHUNK_FILE_MAGIC;
        uint32_t raw_size32 = static_cast<uint32_t>(raw_size);
        uint32_t compressed_size32 = static_cast<uint32_t>(compressed_size);

        file.write(reinterpret_cast<const char*>(&magic), sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(&raw_size32), sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(&compressed_size32), sizeof(uint32_t));
        file.write(compressed.data(), compressed_size);
        file.flush();

        if(!file)
        {
            std::cerr << "Failed to write chunk " << chunk_position << '\n';
            std::error_code error_code;
            std::filesystem::remove(tmp_path, error_code);
            return;
        }
    }

    std::error_code error_code;
    std::filesystem::rename(tmp_path, final_path, error_code);
    if(error_code)
    {
        std::cerr << "Failed to finalize chunk " << chunk_position << ": " << error_code.message() << '\n';
        std::filesystem::remove(tmp_path, error_code);
    }
}

void ServerWorld::saveChunkEntities(int chunk_position)
{
    Chunk& chunk = getChunk(chunk_position);

    BBT root("entities_" + std::to_string(chunk_position));

    for(auto& uuid : chunk.entity_ids)
    {     
        root[entities.at(uuid).getID().toString()] = entities.at(uuid).serialize();
    }

    std::ofstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "entities", std::ios::binary);

    std::vector<uint8_t> buffer = root.save();

    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    stream.close();
}

void ServerWorld::loadChunk(int chunk_position)
{
    loadChunkEnvironment(chunk_position);
    loadChunkEntities(chunk_position);
}

void ServerWorld::loadChunkEnvironment(int chunk_position)
{
    std::filesystem::path chunk_path = path / ("chunk_" + std::to_string(chunk_position)) / "environment";

    size_t expected_raw_size = CHUNK_WIDTH * CHUNK_HEIGHT * (sizeof(BlockID) + sizeof(uint8_t)) + CHUNK_WIDTH * (sizeof(Climate) + sizeof(Biome));

    auto discardCorrupt = [&](const std::string& reason)
    {
        std::cerr << "Discarding corrupt chunk file " << chunk_path
                  << " (" << reason << "); will regenerate\n";
        std::error_code ec;
        std::filesystem::remove(chunk_path, ec);
    };

    std::ifstream file(chunk_path, std::ios::binary);
    if(!file) return;

    uint32_t first_word = 0;
    file.read(reinterpret_cast<char*>(&first_word), sizeof(uint32_t));
    if(file.gcount() != static_cast<std::streamsize>(sizeof(uint32_t)))
    {
        discardCorrupt("header truncated");
        return;
    }

    uint32_t raw_size = 0;
    std::vector<char> compressed;

    if(first_word == CHUNK_FILE_MAGIC)
    {
        uint32_t compressed_size = 0;
        file.read(reinterpret_cast<char*>(&raw_size), sizeof(uint32_t));
        file.read(reinterpret_cast<char*>(&compressed_size), sizeof(uint32_t));
        if(!file)
        {
            discardCorrupt("header incomplete");
            return;
        }
        if(raw_size != expected_raw_size)
        {
            discardCorrupt("raw_size mismatch");
            return;
        }

        compressed.resize(compressed_size);
        if(compressed_size > 0)
        {
            file.read(compressed.data(), compressed_size);
            if(file.gcount() != static_cast<std::streamsize>(compressed_size))
            {
                discardCorrupt("compressed payload truncated");
                return;
            }
        }
    }
    else
    {
        // Legacy format: [raw_size uint32][compressed data to EOF]
        raw_size = first_word;
        if(raw_size != expected_raw_size)
        {
            discardCorrupt("legacy raw_size mismatch");
            return;
        }
        compressed.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    if(compressed.empty())
    {
        discardCorrupt("no compressed data");
        return;
    }

    std::vector<char> raw(raw_size);
    size_t result = ZSTD_decompress(raw.data(), raw_size, compressed.data(), compressed.size());

    if(ZSTD_isError(result))
    {
        discardCorrupt(std::string("ZSTD_decompress: ") + ZSTD_getErrorName(result));
        return;
    }

    if(result != raw_size)
    {
        discardCorrupt("decompressed size mismatch");
        return;
    }

    Chunk& chunk = chunks[chunk_position];
    chunk.chunk_position = chunk_position;
    chunk.generated = true;
    chunk.dirty = false;
    chunk.meshDirty = true;

    const char* ptr = raw.data();

    for(int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for(int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block& block = chunk.blocks[y][x];

            std::memcpy(&block.id, ptr, sizeof(BlockID));
            ptr += sizeof(BlockID);

            std::memcpy(&block.metadata, ptr, sizeof(uint8_t));
            ptr += sizeof(uint8_t);
        }
    }

    for(int x = 0; x < CHUNK_WIDTH; x++)
    {
        Climate& climate = chunk.climates[x];
        Biome& biome = chunk.biomes[x];

        std::memcpy(&climate, ptr, sizeof(Climate));
        ptr += sizeof(Climate);

        std::memcpy(&biome, ptr, sizeof(Biome));
        ptr += sizeof(Biome);

    }
}

void ServerWorld::loadChunkEntities(int chunk_position)
{
    std::ifstream stream(path / ("chunk_" + std::to_string(chunk_position)) / "entities", std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open entities file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    for(auto& [id_string, payload] : root.root())
    {
        Entity entity(uuidFromString(id_string).value());

        entity.deserialize(payload);

        addEntity(std::move(entity));
    }
}

void ServerWorld::savePlayer(UUID entity_id)
{
    Entity& entity = getEntity(entity_id);

    if(!entity.hasComponent<PlayerControlledComponent>()) return;


    auto& player = entity.getComponent<PlayerControlledComponent>();



    BBT root(player.nickname);

    root.root() = entities.at(entity_id).serialize().get<TagCompound>();


    std::ofstream stream(path / (player.nickname), std::ios::binary);

    std::vector<uint8_t> buffer = root.save();

    stream.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

    stream.close();

    std::cout << "Saved entity for player " << player.nickname << '\n';
}


UUID ServerWorld::loadPlayer(std::string nickname)
{
    std::filesystem::path player_path = path / nickname;

    std::ifstream stream(player_path, std::ios::ate | std::ios::binary);

    if(!stream) throw std::runtime_error("Failed to open player file");

    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));

    stream.read(reinterpret_cast<char*>(buffer.data()), size);

    stream.close();


    BBT root = BBT::load(buffer);

    Entity entity(generateUUID());

    Tag tag = Tag(root.root());

    entity.deserialize(tag);

    addEntity(std::move(entity));

    std::cout << "Loaded entity for player " << nickname << '\n';

    return entity.getID();
}

bool ServerWorld::playerFileExist(std::string nickname) const
{
    return std::filesystem::exists(path / (nickname));
}

bool ServerWorld::hasChunkFile(int chunk_position) const
{
    return std::filesystem::exists(path / ("chunk_" + std::to_string(chunk_position)));
}

void ServerWorld::loadOrCreateChunk(int chunk_position)
{
    if (chunks.contains(chunk_position) && chunks.at(chunk_position).generated)
    {
        return;
    }

    if (hasChunkFile(chunk_position))
    {
        loadChunk(chunk_position);
    }
    else
    {

        if (!world_generator)
            throw std::runtime_error("world_generator is NULL");

        chunks[chunk_position] =
            world_generator->generateChunk(chunk_position);
    }
}

sf::Vector2<double> ServerWorld::getSpawnPoint()
{
    loadOrCreateChunk(0);

    sf::Vector2<double> spawnPoint;

    for(int i = 60; i < 255; i++)
    {
        if(getBlock(0, i).id != BlockID::Air && getBlock(0, i + 1).id == BlockID::Air)
        {
            spawnPoint = {0.0f, static_cast<double>(i + 1)};
        }
    }

    return spawnPoint;
}

void ServerWorld::generateWorldSpawn()
{
    for (int i = -2; i <= 2; ++i)
    {
        chunks[i] = world_generator->generateChunk(i);
    }
}