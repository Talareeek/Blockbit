#ifndef CLIENT_WORLD_HPP
#define CLIENT_WORLD_HPP

#include "World.hpp"
#include "ChunkMesh.hpp"

#include <SFML/Graphics/Texture.hpp>

#include <unordered_map>

class ClientWorld : public World
{
    std::unordered_map<int, ChunkMesh> chunk_meshes;

    // One GPU texture per visible chunk, holding the packed lightmap. RGBA8
    // encodes (block_light, sky_light, 0, 255); the fragment shader reads .r
    // and .g. Textures are lazily materialized by Render.cpp. Smooth=true so
    // the hardware does the bilinear blend between tiles.
    std::unordered_map<int, sf::Texture> chunk_light_textures;

public:

    std::unordered_map<int, ChunkMesh>& getChunkMeshes() { return chunk_meshes; }
    const std::unordered_map<int, ChunkMesh>& getChunkMeshes() const { return chunk_meshes; }

    std::unordered_map<int, sf::Texture>& getChunkLightTextures() { return chunk_light_textures; }
};

#endif // CLIENT_WORLD_HPP
