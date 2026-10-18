#ifndef CLIENT_WORLD_HPP
#define CLIENT_WORLD_HPP

#include "World.hpp"
#include "ChunkMesh.hpp"

#include <unordered_map>

class ClientWorld : public World
{
    std::unordered_map<int, ChunkMesh> chunk_meshes;

public:

    std::unordered_map<int, ChunkMesh>& getChunkMeshes() { return chunk_meshes; }
    const std::unordered_map<int, ChunkMesh>& getChunkMeshes() const { return chunk_meshes; }
};

#endif // CLIENT_WORLD_HPP
