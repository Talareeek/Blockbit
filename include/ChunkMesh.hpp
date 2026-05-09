#ifndef CHUNK_MESH_HPP
#define CHUNK_MESH_HPP

#include <SFML/Graphics.hpp>

struct ChunkMesh
{
    sf::VertexArray vertices{sf::PrimitiveType::Triangles};

    bool built{false};
};

#endif // CHUNK_MESH_HPP