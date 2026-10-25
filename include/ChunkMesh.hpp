#ifndef CHUNK_MESH_HPP
#define CHUNK_MESH_HPP

#include <SFML/Graphics.hpp>

struct ChunkMesh
{
    sf::VertexArray vertices{sf::PrimitiveType::Triangles};

    // Overlay mesh for the lighting pass. Same per-block footprint as
    // `vertices` (one quad per non-air block, water quads trimmed to the
    // fluid surface) so the multiply pass darkens only the painted blocks,
    // not the sky showing through air cells.
    sf::VertexArray lightmap_vertices{sf::PrimitiveType::Triangles};

    bool built{false};
};

#endif // CHUNK_MESH_HPP