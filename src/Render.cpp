#include "../include/Render.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/World.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/PlayerControlledComponent.hpp"
#include "../include/Block.hpp"
#include "../include/AssetManager.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/NetworkInterpolationComponent.hpp"
#include "../include/Slot.hpp"
#include "../include/PerlinNoise.hpp"
#include "../include/NormalWorldGenerator.hpp"
#include "../include/DungeonShooterWorldGenerator.hpp"
#include "../include/SkygridWorldGenerator.hpp"
#include "../include/RandomizerWorldGenerator.hpp"
#include "../include/NoodleWorldGenerator.hpp"
#include "../include/WorldLightView.hpp"
#include "../include/LightingSystem.hpp"
#include "../include/Settings.hpp"
#include "../include/ItemUtils.hpp"
#include "../include/ItemInstance.hpp"
#include "../include/ItemDataComponents.hpp"
#include "../include/EffectRegistry.hpp"
#include "../include/Weather.hpp"
#include "../include/ClientWorld.hpp"
#include "../include/LightningBoltComponent.hpp"

#include <memory>
#include <random>
#include <numbers>
#include <unordered_map>
#include <chrono>

namespace
{
    constexpr sf::Color nightTop   {6, 10, 28};
    constexpr sf::Color nightBot   {18, 28, 60};
    constexpr sf::Color dawnTop    {60, 90, 160};
    constexpr sf::Color dawnBot    {220, 150, 150};
    constexpr sf::Color dayTop     {80, 160, 240};
    constexpr sf::Color dayBot     {170, 215, 255};
    constexpr sf::Color sunsetTop  {120, 90, 160};
    constexpr sf::Color sunsetBot  {255, 160, 90};

    float smooth(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // Pack animation into vertex color so a frame change doesn't trigger a
    // mesh rebuild. See resources/shaders/block_animation.frag for the layout:
    //   R = frame_count       G = frame_width_texels
    //   B = frame_duration_cs A = 255
    void appendQuad(sf::VertexArray& vertex_array, const sf::FloatRect& bounds, const sf::IntRect& uv, sf::Color anim)
    {
        sf::Vertex vertex0, vertex1, vertex2, vertex3;

        const sf::Vector2f uvPos  = sf::Vector2f(static_cast<float>(uv.position.x), static_cast<float>(uv.position.y));
        const sf::Vector2f uvSize = sf::Vector2f(static_cast<float>(uv.size.x), static_cast<float>(uv.size.y));

        // Sub-texel inset: atlas tiles are packed with no gutter, so a UV
        // landing exactly on a tile boundary can bleed into the neighbour
        // under rasterizer rounding. A small epsilon (<< 1 texel) is enough
        // to pin nearest-neighbour sampling to this tile without compressing
        // the edge texels — half-texel inset shrinks them to ~1/2 of an
        // interior texel on screen, which is visible as "thin borders".
        constexpr float INSET = 1.0f / 256.0f;
        const float u0 = uvPos.x + INSET;
        const float v0y = uvPos.y + INSET;
        const float u1 = uvPos.x + uvSize.x - INSET;
        const float v1y = uvPos.y + uvSize.y - INSET;

        vertex0.position = {bounds.position.x, bounds.position.y};
        vertex1.position = {bounds.position.x + bounds.size.x, bounds.position.y};
        vertex2.position = {bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
        vertex3.position = {bounds.position.x, bounds.position.y + bounds.size.y};

        vertex0.texCoords = {u0, v1y};
        vertex1.texCoords = {u1, v1y};
        vertex2.texCoords = {u1, v0y};
        vertex3.texCoords = {u0, v0y};

        vertex0.color = anim;
        vertex1.color = anim;
        vertex2.color = anim;
        vertex3.color = anim;

        vertex_array.append(vertex0);
        vertex_array.append(vertex1);
        vertex_array.append(vertex2);

        vertex_array.append(vertex0);
        vertex_array.append(vertex2);
        vertex_array.append(vertex3);
    }

    void appendQuad(sf::VertexArray& vertex_array, const sf::FloatRect& bounds, const sf::IntRect& uv)
    {
        sf::Vertex vertex0, vertex1, vertex2, vertex3;

        const sf::Vector2f uvPos  = sf::Vector2f(static_cast<float>(uv.position.x), static_cast<float>(uv.position.y));
        const sf::Vector2f uvSize = sf::Vector2f(static_cast<float>(uv.size.x), static_cast<float>(uv.size.y));

        // Sub-texel inset: atlas tiles are packed with no gutter, so a UV
        // landing exactly on a tile boundary can bleed into the neighbour
        // under rasterizer rounding. A small epsilon (<< 1 texel) is enough
        // to pin nearest-neighbour sampling to this tile without compressing
        // the edge texels — half-texel inset shrinks them to ~1/2 of an
        // interior texel on screen, which is visible as "thin borders".
        constexpr float INSET = 1.0f / 256.0f;
        const float u0 = uvPos.x + INSET;
        const float v0y = uvPos.y + INSET;
        const float u1 = uvPos.x + uvSize.x - INSET;
        const float v1y = uvPos.y + uvSize.y - INSET;

        vertex0.position = {bounds.position.x, bounds.position.y};
        vertex1.position = {bounds.position.x + bounds.size.x, bounds.position.y};
        vertex2.position = {bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y};
        vertex3.position = {bounds.position.x, bounds.position.y + bounds.size.y};

        vertex0.texCoords = {u0, v1y};
        vertex1.texCoords = {u1, v1y};
        vertex2.texCoords = {u1, v0y};
        vertex3.texCoords = {u0, v0y};

        vertex_array.append(vertex0);
        vertex_array.append(vertex1);
        vertex_array.append(vertex2);

        vertex_array.append(vertex0);
        vertex_array.append(vertex2);
        vertex_array.append(vertex3);
    }

    // Lightmap quad for a single block. Position and texCoord are identical
    // (world chunk-local coords map 1:1 to lightmap pixel coords by design),
    // so GL_LINEAR filtering at the shared block corners produces smooth
    // per-vertex lighting across adjacent cells.
    void appendLightmapQuad(sf::VertexArray& vertex_array, const sf::FloatRect& bounds)
    {
        sf::Vertex v0, v1, v2, v3;
        const float x0 = bounds.position.x;
        const float y0 = bounds.position.y;
        const float x1 = bounds.position.x + bounds.size.x;
        const float y1 = bounds.position.y + bounds.size.y;

        v0.position  = {x0, y0}; v0.texCoords = {x0, y0};
        v1.position  = {x1, y0}; v1.texCoords = {x1, y0};
        v2.position  = {x1, y1}; v2.texCoords = {x1, y1};
        v3.position  = {x0, y1}; v3.texCoords = {x0, y1};
        v0.color = v1.color = v2.color = v3.color = sf::Color::White;

        vertex_array.append(v0);
        vertex_array.append(v1);
        vertex_array.append(v2);

        vertex_array.append(v0);
        vertex_array.append(v2);
        vertex_array.append(v3);
    }

    // Flat-shaded quad used by the sky occluder pass. Texture coords are
    // left at zero (no texture is bound when drawing). Vertex color is a
    // placeholder — RenderSkyOccluder rewrites it every frame with the
    // live sky bottom color.
    void appendSkyOccluderQuad(sf::VertexArray& vertex_array, const sf::FloatRect& bounds)
    {
        sf::Vertex v0, v1, v2, v3;
        const float x0 = bounds.position.x;
        const float y0 = bounds.position.y;
        const float x1 = bounds.position.x + bounds.size.x;
        const float y1 = bounds.position.y + bounds.size.y;

        v0.position = {x0, y0};
        v1.position = {x1, y0};
        v2.position = {x1, y1};
        v3.position = {x0, y1};
        v0.color = v1.color = v2.color = v3.color = sf::Color::White;

        vertex_array.append(v0);
        vertex_array.append(v1);
        vertex_array.append(v2);

        vertex_array.append(v0);
        vertex_array.append(v2);
        vertex_array.append(v3);
    }

    // Shared across RenderWorld and RenderMinimap so animations stay in sync
    // between the main view and the minimap mesh.
    sf::Clock& blockAnimationClock()
    {
        static sf::Clock clock;
        return clock;
    }

    void applyBlockAnimationShader(sf::RenderStates& states)
    {
        sf::Shader& shader = AssetManager::getShader(AssetManager::ShaderID::BlockAnimation);
        const sf::Vector2u atlas_size = BlockAtlas::getTexture().getSize();

        shader.setUniform("texture", sf::Shader::CurrentTexture);
        shader.setUniform("u_time", blockAnimationClock().getElapsedTime().asSeconds());
        shader.setUniform("u_atlas_size", sf::Glsl::Vec2(static_cast<float>(atlas_size.x), static_cast<float>(atlas_size.y)));

        states.shader = &shader;
    }

    // Pack a chunk's lightmap into a flat RGBA buffer: R = block_light * 17,
    // G = sky_light * 17 (so a nibble 0..15 maps exactly to 0..255 — no divide
    // needed in the shader). Row 0 is world y=0 (bottom); the SFML "image top
    // = row 0" convention and the client's Y-flipped view line up so this
    // row ordering samples right-side up on screen.
    std::array<uint8_t, static_cast<size_t>(CHUNK_WIDTH) * static_cast<size_t>(CHUNK_HEIGHT) * 4>
    packChunkLightmap(const ChunkLightData& light)
    {
        std::array<uint8_t, static_cast<size_t>(CHUNK_WIDTH) * static_cast<size_t>(CHUNK_HEIGHT) * 4> pixels{};
        if (light.block_light.empty() || light.sky_light.empty())
        {
            return pixels; // all zero = pitch black until first lighting pass
        }

        constexpr int kScale = 17;  // 15 * 17 = 255
        for (int y = 0; y < CHUNK_HEIGHT; ++y)
        {
            for (int x = 0; x < CHUNK_WIDTH; ++x)
            {
                const size_t src_idx = static_cast<size_t>(y) * CHUNK_WIDTH + x;
                const size_t dst_idx = (static_cast<size_t>(y) * CHUNK_WIDTH + x) * 4;
                pixels[dst_idx + 0] = static_cast<uint8_t>(light.block_light[src_idx] * kScale);
                pixels[dst_idx + 1] = static_cast<uint8_t>(light.sky_light[src_idx]   * kScale);
                pixels[dst_idx + 2] = 0;
                pixels[dst_idx + 3] = 255;
            }
        }
        return pixels;
    }

    // One-time per chunk-position: materialize an RGBA sf::Texture and set
    // GL_LINEAR filtering so the smooth lightmap sampling comes for free.
    // try_emplace constructs sf::Texture in-place to avoid relying on
    // sf::Texture being movable in the current SFML version.
    sf::Texture& ensureLightTexture(ClientWorld& world, int chunk_position)
    {
        auto& textures = world.getChunkLightTextures();
        auto [it, inserted] = textures.try_emplace(
            chunk_position, sf::Vector2u{CHUNK_WIDTH, CHUNK_HEIGHT});
        if (inserted)
        {
            it->second.setSmooth(true);
            it->second.setRepeated(false);
        }
        return it->second;
    }

}

void BuildChunkMesh(ClientWorld& world, int chunk_position)
{
    Chunk& chunk = world.getChunk(chunk_position);
    ChunkMesh& mesh = world.getChunkMeshes()[chunk_position];

    mesh.vertices.clear();
    mesh.lightmap_vertices.clear();
    mesh.sky_occluder_vertices.clear();

    // Per-column highest y that holds a sky-blocking cell. -1 if the column
    // has nothing but lets_light cells. Lets_light cells above this line are
    // sky-exposed and skip the overlay so the sky gradient shows through
    // unchanged; everything else (including lets_light cells with a blocker
    // above) still gets the lightmap applied so the computed light level
    // drives the shading.
    std::array<int, CHUNK_WIDTH> highest_blocker{};
    for (int x = 0; x < CHUNK_WIDTH; ++x)
    {
        highest_blocker[x] = -1;
        for (int y = CHUNK_HEIGHT - 1; y >= 0; --y)
        {
            if (!blockDatabase[chunk.blocks[y][x].id].lets_light)
            {
                highest_blocker[x] = y;
                break;
            }
        }
    }

    for (int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for (int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block& block = chunk.blocks[y][x];

            auto block_data = blockDatabase[block.id];

            const sf::Vector2f position(x, y);

            const bool sky_exposed_lets_light =
                block_data.lets_light && y > highest_blocker[x];
            if (!sky_exposed_lets_light)
            {
                appendLightmapQuad(mesh.lightmap_vertices, {position, {1.0f, 1.0f}});
                appendSkyOccluderQuad(mesh.sky_occluder_vertices, {position, {1.0f, 1.0f}});
            }

            if (!block_data.render.has_value()) continue;

            auto texture_id = block_data.render->texture;

            const sf::IntRect& texture_uv = BlockAtlas::getUV(texture_id);

            const int world_x = chunk_position * CHUNK_WIDTH + x;
            const BlockNeighbors neighbors{
                world.getBlock(world_x,     y + 1).id,
                world.getBlock(world_x + 1, y    ).id,
                world.getBlock(world_x,     y - 1).id,
                world.getBlock(world_x - 1, y    ).id
            };

            const sf::IntRect block_rect = block_data.render->rect(block, neighbors);
            sf::IntRect final_uv = {texture_uv.position + block_rect.position, block_rect.size};

            auto bounds = block_data.render->render_bounds(block);

            sf::FloatRect final_rect = {position + bounds.position, bounds.size};

            

            
            // Static blocks encode frame_count=1 so the shader short-circuits
            // and samples the texCoord unchanged.
            const BlockAnimation anim = block_data.render->animation(block);
            const uint16_t duration_cs = anim.frame_duration_ms / 10u;
            sf::Color anim_color = {
                anim.frame_count,
                static_cast<uint8_t>(std::min<int>(final_uv.size.x, 255)),
                static_cast<uint8_t>(std::min<uint16_t>(duration_cs, 255)),
                255
            };

            if(Settings::graphics.block_animations) appendQuad(mesh.vertices, final_rect, final_uv, anim_color);
            else appendQuad(mesh.vertices, final_rect, final_uv);
        }
    }

    mesh.built = true;
    chunk.meshDirty = false;
}

std::pair<sf::Color, sf::Color> getSkyGradient(float t)
{
    t = std::fmod(t, 1.0f);

    sf::Color top, bottom;

    if (t < 0.25f)
    {
        float k = smooth(t / 0.25f);
        top    = lerpColor(nightTop, dawnTop, k);
        bottom = lerpColor(nightBot, dawnBot, k);
    }
    else if (t < 0.5f)
    {
        float k = smooth((t - 0.25f) / 0.25f);
        top    = lerpColor(dawnTop, dayTop, k);
        bottom = lerpColor(dawnBot, dayBot, k);
    }
    else if (t < 0.75f)
    {
        float k = smooth((t - 0.5f) / 0.25f);
        top    = lerpColor(dayTop, sunsetTop, k);
        bottom = lerpColor(dayBot, sunsetBot, k);
    }
    else
    {
        float k = smooth((t - 0.75f) / 0.25f);
        top    = lerpColor(sunsetTop, nightTop, k);
        bottom = lerpColor(sunsetBot, nightBot, k);
    }

    return {top, bottom};
}

std::pair<sf::Color, sf::Color> getSkyGradientWeathered(float t, float rain_intensity, float thunder_intensity)
{
    auto [top, bottom] = getSkyGradient(t);

    if (rain_intensity <= 0.0f && thunder_intensity <= 0.0f) return {top, bottom};

    // Clamp so an overshoot from the fade curve can't push colors out of range.
    const float rain_k    = std::clamp(rain_intensity,    0.0f, 1.0f);
    const float thunder_k = std::clamp(thunder_intensity, 0.0f, 1.0f);

    constexpr sf::Color rain_tint    {80, 85, 95};
    constexpr sf::Color thunder_tint {30, 32, 38};

    sf::Color storm_top    = lerpColor(rain_tint,    thunder_tint, thunder_k);
    sf::Color storm_bottom = lerpColor(rain_tint,    thunder_tint, thunder_k);

    // Clamp per channel so weather can only darken the sky — otherwise rain
    // at night would lerp towards the brighter tint and lift sky luminance.
    storm_top.r    = std::min(storm_top.r,    top.r);
    storm_top.g    = std::min(storm_top.g,    top.g);
    storm_top.b    = std::min(storm_top.b,    top.b);
    storm_bottom.r = std::min(storm_bottom.r, bottom.r);
    storm_bottom.g = std::min(storm_bottom.g, bottom.g);
    storm_bottom.b = std::min(storm_bottom.b, bottom.b);

    // Rain intensity drives most of the shift; thunder deepens it further.
    const float blend = std::clamp(rain_k * 0.75f + thunder_k * 0.20f, 0.0f, 0.95f);

    top    = lerpColor(top,    storm_top,    blend);
    bottom = lerpColor(bottom, storm_bottom, blend);

    return {top, bottom};
}

void renderSky(sf::RenderTarget& target, sf::Color top, sf::Color bottom)
{
    const float width = static_cast<float>(target.getSize().x);
    const float height = static_cast<float>(target.getSize().y);

    sf::VertexArray sky(sf::PrimitiveType::TriangleStrip, 4);

    sky[0].position = {0.0f, 0.0f};
    sky[1].position = {width,    0.0f};
    sky[2].position = {0.0f, height};
    sky[3].position = {width,    height};

    sky[0].color = top;
    sky[1].color = top;
    sky[2].color = bottom;
    sky[3].color = bottom;

    sf::View previous = target.getView();
    target.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {width, height})));
    target.draw(sky);
    target.setView(previous);
}

namespace
{
    // Horizontal radius around the camera, in blocks, inside which we draw
    // precipitation. Keep it modest — most rain beyond that is hidden by
    // screen edges at 1x view scale anyway, and this is per-frame work.
    constexpr int PRECIP_RADIUS = 24;

    // Vertical span over which a drop lives above the top opaque block it
    // targets. Big enough to always start off-screen for a normal view height,
    // small enough that we don't scan the whole chunk column for a roof.
    constexpr float PRECIP_FALL_RANGE = 24.0f;

    // Hash a column x to a stable float in [0,1). Phase offsets per column
    // scatter drops so they don't move in sync.
    float columnPhase(int wx, int seed)
    {
        uint32_t h = static_cast<uint32_t>(wx * 73856093) ^ static_cast<uint32_t>(seed * 19349663);
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return (h & 0xFFFFFF) / static_cast<float>(0x1000000);
    }

    // Walk straight down from the chunk top until we hit a non-Air block. Not
    // the same as the lighting heightmap, but close enough for drop clipping
    // and keeps the renderer free of lighting internals.
    int topOpaqueY(ClientWorld& world, int wx, int y_ceiling)
    {
        const int y_start = std::min(y_ceiling, CHUNK_HEIGHT - 1);
        for (int y = y_start; y >= 0; --y)
        {
            if (world.getBlock(wx, y).id != BlockID::Air) return y;
        }
        return -1;
    }
}

void renderPrecipitation(ClientWorld& world, sf::Vector2<double> camera,
                         float rain_intensity, float thunder_intensity,
                         sf::RenderWindow& window)
{
    const float intensity = std::clamp(rain_intensity, 0.0f, 1.0f);
    if (intensity <= 0.001f) return;

    // Shared wall-clock timer so rain speed is independent of the simulation
    // tick. SFML's own clock would also work, but a chrono one survives any
    // target-rate changes without resetting drop positions.
    static auto time_origin = std::chrono::steady_clock::now();
    const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - time_origin).count();

    const float unit_size = window.getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    // Screen half-height in blocks under the current view. Drops outside this
    // band are rejected cheaply instead of being drawn offscreen.
    const sf::View view = window.getView();
    const float screen_half_h_blocks = std::abs(view.getSize().y) * 0.5f / unit_size;
    const float screen_half_w_blocks = view.getSize().x * 0.5f / unit_size;

    const int min_x = static_cast<int>(std::floor(camera.x - std::max<float>(PRECIP_RADIUS, screen_half_w_blocks + 2.0f)));
    const int max_x = static_cast<int>(std::ceil (camera.x + std::max<float>(PRECIP_RADIUS, screen_half_w_blocks + 2.0f)));

    const int y_ceiling = static_cast<int>(std::ceil(camera.y + screen_half_h_blocks + PRECIP_FALL_RANGE));

    // Thunder darkens rain, stormy sky makes bright drops read better.
    const int rain_alpha = static_cast<int>(std::round(160.0f * intensity));
    const int snow_alpha = static_cast<int>(std::round(200.0f * intensity));
    const sf::Color rain_color(170 - static_cast<int>(40 * thunder_intensity),
                               190 - static_cast<int>(40 * thunder_intensity),
                               255,
                               static_cast<uint8_t>(std::clamp(rain_alpha, 0, 255)));
    const sf::Color snow_color(240, 245, 255, static_cast<uint8_t>(std::clamp(snow_alpha, 0, 255)));

    constexpr float RAIN_FALL_SPEED = 22.0f;   // blocks per second
    constexpr float SNOW_FALL_SPEED = 2.5f;

    // Drop count per column scales with intensity so rain doesn't appear and
    // disappear discretely during the fade.
    const int rain_drops_per_col = std::max(1, static_cast<int>(std::round(3.0f * intensity)));
    const int snow_drops_per_col = std::max(1, static_cast<int>(std::round(2.0f * intensity)));

    sf::VertexArray vertices(sf::PrimitiveType::Triangles);
    vertices.resize(0);

    auto& chunks = world.getChunks();

    for (int wx = min_x; wx <= max_x; ++wx)
    {
        const int chunk_position = (wx >= 0) ? wx / CHUNK_WIDTH : (wx - CHUNK_WIDTH + 1) / CHUNK_WIDTH;
        auto chunk_it = chunks.find(chunk_position);
        if (chunk_it == chunks.end() || !chunk_it->second.generated) continue;

        const int local_x = wx - chunk_position * CHUNK_WIDTH;
        const Biome biome = chunk_it->second.biomes[local_x];
        const Precipitation precip = precipitationFor(WeatherState::Rain, biome);
        if (precip == Precipitation::None) continue;

        const int top_y = topOpaqueY(world, wx, y_ceiling);
        if (top_y < 0) continue;

        const float spawn_bottom = static_cast<float>(top_y + 1);
        const float spawn_top    = spawn_bottom + PRECIP_FALL_RANGE;

        // Column entirely above or below the visible band -> nothing to draw.
        const float view_top    = static_cast<float>(camera.y + screen_half_h_blocks);
        const float view_bottom = static_cast<float>(camera.y - screen_half_h_blocks);
        if (spawn_bottom > view_top)   continue;
        if (spawn_top    < view_bottom) continue;

        const bool is_snow = (precip == Precipitation::Snow);
        const sf::Color color = is_snow ? snow_color : rain_color;
        const float speed = is_snow ? SNOW_FALL_SPEED : RAIN_FALL_SPEED;
        const int drops = is_snow ? snow_drops_per_col : rain_drops_per_col;
        const float drop_length = is_snow ? 0.16f : 0.55f;
        const float drop_width  = is_snow ? 0.14f : 0.05f;

        for (int d = 0; d < drops; ++d)
        {
            const float seed = columnPhase(wx, d * 131 + (is_snow ? 7 : 3));

            float phase = seed + t * speed / PRECIP_FALL_RANGE;
            phase = phase - std::floor(phase);  // wrap to [0,1)

            const float drop_y = spawn_top - phase * PRECIP_FALL_RANGE;

            if (drop_y + drop_length < view_bottom) continue;
            if (drop_y > view_top) continue;

            // Jitter within the 1-block column; snow sways sideways with time.
            float jitter_x = seed;
            if (is_snow)
            {
                jitter_x = 0.5f + 0.35f * std::sin(t * 0.8f + seed * 6.283f);
            }

            const float px0 = static_cast<float>(wx) + jitter_x - drop_width * 0.5f;
            const float px1 = px0 + drop_width;
            const float py_top = drop_y;
            const float py_bot = drop_y - drop_length;

            sf::Vertex v0, v1, v2, v3;
            v0.position = {px0, py_top}; v0.color = color;
            v1.position = {px1, py_top}; v1.color = color;
            v2.position = {px1, py_bot}; v2.color = color;
            v3.position = {px0, py_bot}; v3.color = color;

            vertices.append(v0);
            vertices.append(v1);
            vertices.append(v2);
            vertices.append(v0);
            vertices.append(v2);
            vertices.append(v3);
        }
    }

    if (vertices.getVertexCount() == 0) return;

    // Vertices are in world coordinates, so apply the same camera->screen
    // transform BuildChunkMesh uses for terrain.
    sf::RenderStates states;
    states.transform.translate({static_cast<float>(-camera.x) * unit_size, static_cast<float>(-camera.y) * unit_size});
    states.transform.scale({unit_size, unit_size});

    window.draw(vertices, states);
}

namespace
{
    uint32_t hash32(uint32_t x)
    {
        x ^= x >> 16;
        x *= 0x7feb352du;
        x ^= x >> 15;
        x *= 0x846ca68bu;
        x ^= x >> 16;
        return x;
    }
}

void renderLightningEntities(World& world, sf::Vector2<double> camera, sf::RenderWindow& window)
{
    const auto& entities = world.getEntities();

    // Cheap early-out — the common case is no lightning anywhere.
    bool any = false;
    for (const auto& [id, entity] : entities)
    {
        if (entity.hasComponent<LightningBoltComponent>()) { any = true; break; }
    }
    if (!any) return;

    const float unit_size = window.getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    sf::VertexArray tris(sf::PrimitiveType::Triangles);

    for (const auto& [id, entity] : entities)
    {
        if (!entity.hasComponent<LightningBoltComponent>()) continue;
        if (!entity.hasComponent<TransformComponent>())     continue;

        const auto& transform = entity.getComponent<TransformComponent>();
        const auto& bolt      = entity.getComponent<LightningBoltComponent>();

        // Fade out as the lifetime depletes. Server starts it at 0.5s and
        // ticks it down to zero before despawn.
        const float fade = std::clamp(bolt.lifetime_remaining / 0.5f, 0.0f, 1.0f);
        const uint8_t alpha = static_cast<uint8_t>(std::round(230.0f * fade));
        const sf::Color core_color(240, 240, 255, alpha);
        const sf::Color halo_color(180, 210, 255, static_cast<uint8_t>(alpha / 3));

        const float base_x = static_cast<float>(transform.position.x) + static_cast<float>(transform.size.x) * 0.5f;
        const float base_y = static_cast<float>(transform.position.y);

        // Procedural zigzag: walk from the top of the bolt down to the base,
        // kinking left/right by a hashed amount per segment. Seed-stable so
        // every client draws the same shape for the same strike.
        constexpr int SEGMENTS = 14;
        const float dy = bolt.height / static_cast<float>(SEGMENTS);

        float prev_x = base_x;
        float prev_y = base_y + bolt.height;

        for (int i = 0; i < SEGMENTS; ++i)
        {
            const float ny = prev_y - dy;
            const uint32_t h = hash32(bolt.seed + static_cast<uint32_t>(i));
            // Normalize hash to [-1, 1], then shrink kinks as we approach the
            // ground so the bolt ends on-target instead of drifting away.
            float jitter = ((h & 0xFFFF) / 32768.0f) - 1.0f;
            const float progress = static_cast<float>(i) / SEGMENTS;
            jitter *= (1.0f - progress) * 0.9f + 0.1f;

            float nx = (i == SEGMENTS - 1) ? base_x : (base_x + jitter);

            // Two quads per segment: a thick halo and a thin bright core. Both
            // are axis-aligned rects along the segment — the diagonal isn't
            // perfectly perpendicular to the segment, but it's close enough
            // for a zigzag that draws for <0.5s.
            auto appendSegment = [&](sf::Color color, float thickness)
            {
                const float half = thickness * 0.5f;

                sf::Vertex v0, v1, v2, v3;
                v0.position = {prev_x - half, prev_y}; v0.color = color;
                v1.position = {prev_x + half, prev_y}; v1.color = color;
                v2.position = {nx    + half, ny};      v2.color = color;
                v3.position = {nx    - half, ny};      v3.color = color;

                tris.append(v0); tris.append(v1); tris.append(v2);
                tris.append(v0); tris.append(v2); tris.append(v3);
            };

            appendSegment(halo_color, 0.55f);
            appendSegment(core_color, 0.15f);

            prev_x = nx;
            prev_y = ny;
        }
    }

    if (tris.getVertexCount() == 0) return;

    sf::RenderStates states;
    states.transform.translate({static_cast<float>(-camera.x) * unit_size, static_cast<float>(-camera.y) * unit_size});
    states.transform.scale({unit_size, unit_size});
    states.blendMode = sf::BlendAdd;

    window.draw(tris, states);
}

void renderLightningFlash(float intensity, sf::RenderTarget& target)
{
    const float k = std::clamp(intensity, 0.0f, 1.0f);
    if (k <= 0.0f) return;

    const float width  = static_cast<float>(target.getSize().x);
    const float height = static_cast<float>(target.getSize().y);

    const uint8_t alpha = static_cast<uint8_t>(std::round(180.0f * k));
    const sf::Color flash_color(255, 255, 255, alpha);

    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0].position = {0.0f, 0.0f};
    quad[1].position = {width, 0.0f};
    quad[2].position = {0.0f, height};
    quad[3].position = {width, height};
    for (int i = 0; i < 4; ++i) quad[i].color = flash_color;

    sf::View previous = target.getView();
    target.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {width, height})));
    target.draw(quad);
    target.setView(previous);
}

void renderSunAndMoon(float phase, sf::RenderWindow& window)
{
    sf::Shader& shader = AssetManager::getShader(AssetManager::ShaderID::Sun);

    float sun_angle  = phase * 2.0f * static_cast<float>(std::numbers::pi);
    float moon_angle = sun_angle + static_cast<float>(std::numbers::pi);

    float W = static_cast<float>(window.getSize().x);
    float H = static_cast<float>(window.getSize().y);

    float radius   = W * 0.55f;
    float bodySize = W / 24.0f;

    auto drawBody = [&](float angle, sf::Color core)
    {
        float bx = -std::sin(angle) * radius + W * 0.5f;
        float by =  std::cos(angle) * radius + H;

        float glow = 10.f;
        float quadSize = bodySize + glow * 2.0f;

        sf::RectangleShape body({quadSize, quadSize});
        body.setFillColor(core);
        body.setOrigin({quadSize * 0.5f, quadSize * 0.5f});
        body.setPosition({bx, by});

        shader.setUniform("resolution", sf::Glsl::Vec2(W, H));
        shader.setUniform("sunCenter", sf::Glsl::Vec2(bx, H - by));
        shader.setUniform("sunSize", bodySize * 0.5f);
        shader.setUniform("sunColor", sf::Glsl::Vec4(
            core.r / 255.f, core.g / 255.f, core.b / 255.f, core.a / 255.f));
        shader.setUniform("glowSize", glow);

        window.draw(body, &shader);
    };

    sf::View previous = window.getView();

    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    drawBody(moon_angle, sf::Color(235, 235, 245));
    drawBody(sun_angle,  sf::Color(255, 235, 140));

    window.setView(previous);
}

sf::Vector2f getMouseWorldPosition(const World&, const sf::RenderWindow& window)
{
    float unit_size = window.getView().getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    return {-(mouseWorld.x / unit_size), -(mouseWorld.y / unit_size)};
}

sf::Vector2i getMouseBlockPosition(const World& world, const sf::RenderWindow& window)
{
    auto mousePos = getMouseWorldPosition(world, window);
    return {static_cast<int>(std::floor(mousePos.x)), static_cast<int>(std::floor(mousePos.y))};
}

sf::Vector2f getSunWorldPosition(const World& world, sf::Vector2f cameraCenter)
{
    constexpr float PI = 3.14159265359f;
    constexpr float SUN_DISTANCE = 200.0f;

    float t = solarPhase(world.getDayTime(), world.days);

    float normalized = std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f);
    float angle = normalized * PI;

    return
    {
        cameraCenter.x - std::cos(angle) * SUN_DISTANCE,
        cameraCenter.y + std::sin(angle) * SUN_DISTANCE
    };
}

void RenderSkyOccluder(ClientWorld& world, const sf::Vector2<double> camera, sf::RenderWindow& window, sf::Color color)
{
    // Paints the sky bottom color over every non-sky-exposed cell in world
    // space, before the chunk mesh is drawn. The screen-space sky/stars pass
    // below is then only visible through the "hole" above the terrain —
    // caves and roofed air no longer leak starlight through to the camera.
    // The lightmap multiply in RenderWorld later darkens the painted area
    // down to near-black wherever sky_light drops to zero.
    float unit_size = window.getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    int centerChunk = static_cast<int>(std::floor(camera.x / static_cast<double>(CHUNK_WIDTH)));

    for (int i = centerChunk - 2; i <= centerChunk + 2; ++i)
    {
        Chunk& chunk = world.getChunk(i);
        if (!chunk.generated) continue;

        ChunkMesh& mesh = world.getChunkMeshes()[i];
        if (chunk.meshDirty || !mesh.built)
        {
            BuildChunkMesh(world, i);
        }

        const std::size_t vertex_count = mesh.sky_occluder_vertices.getVertexCount();
        if (vertex_count == 0) continue;

        for (std::size_t v = 0; v < vertex_count; ++v)
        {
            mesh.sky_occluder_vertices[v].color = color;
        }

        float translate_x = static_cast<float>(static_cast<double>(i * CHUNK_WIDTH) - camera.x) * unit_size;
        float translate_y = static_cast<float>(-camera.y) * unit_size;

        sf::RenderStates states;
        states.transform.translate({translate_x, translate_y});
        states.transform.scale({unit_size, unit_size});

        window.draw(mesh.sky_occluder_vertices, states);
    }
}

void RenderWorld(ClientWorld& world, const sf::Vector2<double> camera, sf::RenderWindow& window)
{
    float unit_size = window.getSize().y / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    int centerChunk = static_cast<int>(std::floor(camera.x / static_cast<double>(CHUNK_WIDTH)));

    const float day_factor   = computeDayFactor(solarPhase(world.getDayTime(), world.days));
    // Sky light runs from max=15 at noon down to min=4 at midnight (4/15 floor).
    // ambient_min stays at 0 so sky-exposed tiles can actually reach the "4"
    // floor at midnight instead of being clamped higher by ambient — unlit
    // caves still get some brightness via block-penetrating light.
    constexpr float ambient_min      = 0.0f;
    constexpr float night_sky_floor  = 4.0f / 15.0f;
    const     float effect_strength  = std::clamp(Settings::graphics.lighting_effect_strength, 0.0f, 1.0f);

    for (int i = centerChunk - 2; i <= centerChunk + 2; ++i)
    {
        Chunk& chunk = world.getChunk(i);

        if (!chunk.generated) continue;

        ChunkMesh& mesh = world.getChunkMeshes()[i];

        if (chunk.meshDirty || !mesh.built)
        {
            BuildChunkMesh(world, i);
        }

        // Lazy-init lighting the first time we see this chunk on the client.
        // We rely on the same deterministic BFS the server runs, so the
        // lightmap derived here will match what the server would have
        // computed from the same blocks.
        //
        // When a neighbor is already lit we re-light the group together: zero
        // every member's lightmap and seed every column in one pass before a
        // single BFS. Lighting the new chunk alone with a cross-flood from
        // the neighbor's edge leaves a visible seam because the neighbor's
        // edge was itself computed when this chunk didn't exist — regenerating
        // from scratch makes the border propagate with full context.
        if (!chunk.light.initialized)
        {
            auto& all_chunks = world.getChunks();

            std::vector<int> group;
            group.push_back(i);
            for (int neighbor_cp : {i - 1, i + 1})
            {
                auto n_it = all_chunks.find(neighbor_cp);
                if (n_it == all_chunks.end()) continue;
                if (!n_it->second.light.initialized) continue;
                group.push_back(neighbor_cp);
            }

            for (int cp : group)
            {
                auto it = all_chunks.find(cp);
                if (it == all_chunks.end()) continue;
                auto& light = it->second.light;
                if (!light.block_light.empty())
                    std::fill(light.block_light.begin(), light.block_light.end(), 0);
                if (!light.sky_light.empty())
                    std::fill(light.sky_light.begin(), light.sky_light.end(), 0);
                light.texture_dirty = true;
            }

            WorldLightView view(world);
            initializeChunkGroupSkyLight(view, group, CHUNK_WIDTH, CHUNK_HEIGHT);

            for (int cp : group)
            {
                auto it = all_chunks.find(cp);
                if (it != all_chunks.end()) it->second.light.initialized = true;
            }
        }

        float translate_x = static_cast<float>((static_cast<double>(i * CHUNK_WIDTH) - camera.x)) * unit_size;
        float translate_y = static_cast<float>(-camera.y) * unit_size;

        sf::RenderStates states;
        states.texture = &BlockAtlas::getTexture();
        states.transform.translate({translate_x, translate_y});
        states.transform.scale({unit_size, unit_size});
        if(Settings::graphics.block_animations) applyBlockAnimationShader(states);

        window.draw(mesh.vertices, states);

        if(Settings::graphics.lighting_effect)
        {
            // --- Lighting overlay pass ---------------------------------------
            // Multiply blend: dst_new = overlay_rgb * dst_old. The shader outputs
            // the final light factor (max of block/sky light, clamped against
            // ambient_min). One extra draw call per visible chunk.
            sf::Texture& light_tex = ensureLightTexture(world, i);
            if (chunk.light.texture_dirty)
            {
                auto pixels = packChunkLightmap(chunk.light);
                light_tex.update(pixels.data());
                chunk.light.texture_dirty = false;
            }

            sf::RenderStates overlay_states;

            sf::Shader& lighting_shader = AssetManager::getShader(AssetManager::ShaderID::LightingOverlay);
            lighting_shader.setUniform("texture", sf::Shader::CurrentTexture);
            lighting_shader.setUniform("u_day_factor",      day_factor);
            lighting_shader.setUniform("u_ambient_min",     ambient_min);
            lighting_shader.setUniform("u_night_sky_floor", night_sky_floor);
            lighting_shader.setUniform("u_effect_strength", effect_strength);

            overlay_states.shader    = &lighting_shader;        
            overlay_states.texture   = &light_tex;
            overlay_states.transform = states.transform;  // same chunk-to-screen mapping
            overlay_states.blendMode = sf::BlendMultiply;

            window.draw(mesh.lightmap_vertices, overlay_states);
        }
    }
}

void RenderBlockOverlay(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window, UUID viewerEntityId)
{
    sf::View view({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});

    view.setSize({view.getSize().x, -view.getSize().y});

    window.setView(view);

    unsigned int unit_size = window.getSize().y / WORLD_UNIT_SIZE_FACTOR;

    sf::Vector2f mouseView = window.mapPixelToCoords(sf::Mouse::getPosition(window));
    double mouseWorldX = static_cast<double>(mouseView.x) / static_cast<double>(unit_size) + camera.x;
    double mouseWorldY = static_cast<double>(mouseView.y) / static_cast<double>(unit_size) + camera.y;
    int blockX = static_cast<int>(std::floor(mouseWorldX));
    int blockY = static_cast<int>(std::floor(mouseWorldY));
    sf::Vector2i blockPos{blockX, blockY};

    if(world.getBlock(blockX, blockY).id != BlockID::Air && isBlockInRange(world.getEntity(viewerEntityId).getComponent<TransformComponent>(), blockPos, 4.0f))
    {
        sf::Sprite sprite(AssetManager::getUITexture(AssetManager::UITextureID::Block_Overlay));
        sprite.setPosition({
            static_cast<float>((static_cast<double>(blockX) - camera.x) * static_cast<double>(unit_size)),
            static_cast<float>((static_cast<double>(blockY) - camera.y) * static_cast<double>(unit_size))
        });

        sprite.setScale({
            (1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.x,
            -((1 * static_cast<float>(unit_size)) / sprite.getTextureRect().size.y)
        });

        sprite.setOrigin({
            0.f,
            static_cast<float>(sprite.getTextureRect().size.y)
        });

        window.draw(sprite);
    }
}

#include <iostream>

void RenderBlockOutline(World& world, const sf::Vector2<double> camera, const sf::Vector2i block, UUID viewerEntityId, sf::RenderWindow& window)
{
    constexpr float TRANSITION_DURATION = 0.1f;
    constexpr float OPACITY_TRANSITION_DURATION = 0.15f;
    constexpr float INSET = 1.0f / 16.0f;

    static sf::Rect<double> source_rect{};
    static sf::Rect<double> target_rect{};
    static sf::Rect<double> current_rect{};
    static float transition_elapsed = TRANSITION_DURATION;
    static float current_opacity = 1.0f;
    static bool initialized = false;
    static sf::Clock clock;

    const float dt = clock.restart().asSeconds();

    {
        if (!world.doesEntityExist(viewerEntityId)) return;
        auto& viewer = world.getEntity(viewerEntityId);
        if (!viewer.hasComponent<TransformComponent>()) return;
    }

    const Block hovered_block = world.getBlock(block.x, block.y);
    const bool is_air = hovered_block.id == BlockID::Air;

    auto resolveBounds = [&]() -> sf::FloatRect
    {
        auto it = blockDatabase.find(hovered_block.id);
        if (it != blockDatabase.end() && it->second.render.has_value())
        {
            sf::FloatRect r = it->second.render->render_bounds(hovered_block);
            if (r.size.x > 0.0f && r.size.y > 0.0f) return r;
        }
        return {{0.0f, 0.0f}, {1.0f, 1.0f}};
    };

    sf::FloatRect local = resolveBounds();
    sf::Rect<double> target
    {
        {static_cast<double>(block.x) + static_cast<double>(local.position.x),
         static_cast<double>(block.y) + static_cast<double>(local.position.y)},
        {static_cast<double>(local.size.x), static_cast<double>(local.size.y)}
    };

    const float target_opacity = is_air ? std::clamp(1.0f - Settings::render.selector_air_fade, 0.0f, 1.0f) : 1.0f;

    const bool target_changed =
        target.position.x != target_rect.position.x ||
        target.position.y != target_rect.position.y ||
        target.size.x     != target_rect.size.x     ||
        target.size.y     != target_rect.size.y;

    if (!initialized)
    {
        source_rect = target;
        target_rect = target;
        current_rect = target;
        transition_elapsed = TRANSITION_DURATION;
        current_opacity = target_opacity;
        initialized = true;
    }
    else if (target_changed)
    {
        source_rect = current_rect;
        target_rect = target;
        transition_elapsed = 0.0f;
    }

    if (transition_elapsed < TRANSITION_DURATION)
    {
        transition_elapsed = std::min(transition_elapsed + dt, TRANSITION_DURATION);
        double t = static_cast<double>(transition_elapsed / TRANSITION_DURATION);
        t = t * t * (3.0 - 2.0 * t);

        current_rect.position.x = source_rect.position.x + (target_rect.position.x - source_rect.position.x) * t;
        current_rect.position.y = source_rect.position.y + (target_rect.position.y - source_rect.position.y) * t;
        current_rect.size.x     = source_rect.size.x     + (target_rect.size.x     - source_rect.size.x)     * t;
        current_rect.size.y     = source_rect.size.y     + (target_rect.size.y     - source_rect.size.y)     * t;
    }
    else
    {
        current_rect = target_rect;
    }

    if (OPACITY_TRANSITION_DURATION > 0.0f && dt > 0.0f)
    {
        const float max_step = dt / OPACITY_TRANSITION_DURATION;
        const float delta = target_opacity - current_opacity;
        current_opacity += std::clamp(delta, -max_step, max_step);
    }
    else
    {
        current_opacity = target_opacity;
    }

    if (current_opacity <= 0.0f) return;

    float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    double inset = std::min(static_cast<double>(INSET), 0.5 * std::min(current_rect.size.x, current_rect.size.y));

    sf::RectangleShape shape(
        {
            static_cast<float>((current_rect.size.x - 2.0 * inset) * unit_size),
            static_cast<float>((current_rect.size.y - 2.0 * inset) * unit_size)
        }
    );

    sf::Color outline_color = Settings::render.selector_color;
    outline_color.a = static_cast<std::uint8_t>(std::round(255.0f * std::clamp(current_opacity, 0.0f, 1.0f)));

    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(outline_color);
    shape.setOutlineThickness(static_cast<float>(inset * unit_size));

    shape.setPosition(
        {
            static_cast<float>((current_rect.position.x - camera.x + inset) * unit_size),
            static_cast<float>((current_rect.position.y - camera.y + inset) * unit_size)
        }
    );

    window.draw(shape);
}

void RenderEntityHitboxes(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window)
{
    const float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);
    const float thickness = std::max(1.0f, unit_size / 16.0f);

    sf::VertexArray lines(sf::PrimitiveType::Triangles);

    auto pushLine = [&](sf::Vector2f a, sf::Vector2f b, sf::Color color)
    {
        sf::Vector2f dir = b - a;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len <= 0.0f) return;
        sf::Vector2f n{-dir.y / len, dir.x / len};
        sf::Vector2f off = n * (thickness * 0.5f);

        sf::Vertex v0{a - off, color}; v0.texCoords = {0.f, 0.f};
        sf::Vertex v1{b - off, color}; v1.texCoords = {0.f, 0.f};
        sf::Vertex v2{b + off, color}; v2.texCoords = {0.f, 0.f};
        sf::Vertex v3{a + off, color}; v3.texCoords = {0.f, 0.f};

        lines.append(v0); lines.append(v1); lines.append(v2);
        lines.append(v0); lines.append(v2); lines.append(v3);
    };

    const sf::Color color(0, 255, 0);

    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<TransformComponent>()) continue;
        auto& transform = entity.getComponent<TransformComponent>();

        const float x0 = static_cast<float>(transform.render_position.x - camera.x) * unit_size;
        const float y0 = static_cast<float>(transform.render_position.y - camera.y) * unit_size;
        const float x1 = x0 + static_cast<float>(transform.size.x) * unit_size;
        const float y1 = y0 + static_cast<float>(transform.size.y) * unit_size;

        pushLine({x0, y0}, {x1, y0}, color);
        pushLine({x1, y0}, {x1, y1}, color);
        pushLine({x1, y1}, {x0, y1}, color);
        pushLine({x0, y1}, {x0, y0}, color);
    }

    if (lines.getVertexCount() > 0) window.draw(lines);
}

void RenderBorder(const sf::Vector2<double> camera, float view_scale, sf::RenderWindow& window)
{
    const float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);
    const float half_w = static_cast<float>(window.getSize().x) * view_scale * 0.5f;
    const float half_h = static_cast<float>(window.getSize().y) * view_scale * 0.5f;

    sf::Texture& tex = AssetManager::getUITexture(AssetManager::UITextureID::Border);
    const float tex_w = static_cast<float>(tex.getSize().x);
    const float tex_h = static_cast<float>(tex.getSize().y);

    const float y_bottom = -half_h;
    const float y_top    =  half_h;
    // One texture copy per world unit in both axes so stripe spacing matches
    // the block grid regardless of zoom.
    const float v_max    = (y_top - y_bottom) / unit_size * tex_h;

    auto drawWall = [&](float x_left, float x_right, float u_left, float u_right)
    {
        if (x_left >= x_right) return;

        sf::VertexArray quad(sf::PrimitiveType::Triangles, 6);
        sf::Vertex bl, br, tr, tl;
        bl.position = {x_left,  y_bottom};  bl.texCoords = {u_left,  v_max};
        br.position = {x_right, y_bottom};  br.texCoords = {u_right, v_max};
        tr.position = {x_right, y_top};     tr.texCoords = {u_right, 0.0f};
        tl.position = {x_left,  y_top};     tl.texCoords = {u_left,  0.0f};
        bl.color = br.color = tr.color = tl.color = sf::Color(255, 255, 255, 60);

        quad[0] = bl; quad[1] = br; quad[2] = tr;
        quad[3] = bl; quad[4] = tr; quad[5] = tl;

        sf::RenderStates states;
        states.texture = &tex;
        window.draw(quad, states);
    };

    // Positive border: fills [BORDER_X, +inf). U starts at the clipped-left
    // edge offset so tiling stays aligned with the world coord grid when the
    // strip is partially off-screen on the left.
    const float pos_inner_screen = static_cast<float>((static_cast<double>(World::BORDER_X) - camera.x) * static_cast<double>(unit_size));
    if (pos_inner_screen < half_w)
    {
        const float x_left  = std::max(pos_inner_screen, -half_w);
        const float x_right = half_w;
        const float u_left  = (x_left  - pos_inner_screen) / unit_size * tex_w;
        const float u_right = (x_right - pos_inner_screen) / unit_size * tex_w;
        drawWall(x_left, x_right, u_left, u_right);
    }

    // Negative border: fills (-inf, -BORDER_X]. Tile the texture outward from
    // the inner edge (rightmost world coord of the wall) so the stripe pattern
    // mirrors the positive side.
    const float neg_inner_screen = static_cast<float>((static_cast<double>(-World::BORDER_X) - camera.x) * static_cast<double>(unit_size));
    if (neg_inner_screen > -half_w)
    {
        const float x_left  = -half_w;
        const float x_right = std::min(neg_inner_screen, half_w);
        const float u_left  = (neg_inner_screen - x_left ) / unit_size * tex_w;
        const float u_right = (neg_inner_screen - x_right) / unit_size * tex_w;
        drawWall(x_left, x_right, u_left, u_right);
    }
}

void RenderChunkBorders(const sf::Vector2<double> camera, sf::RenderWindow& window)
{
    const float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);
    const float thickness = std::max(1.0f, unit_size / 16.0f);

    const int centerChunk = static_cast<int>(std::floor(camera.x / static_cast<double>(CHUNK_WIDTH)));

    sf::VertexArray lines(sf::PrimitiveType::Triangles);

    auto pushLine = [&](sf::Vector2f a, sf::Vector2f b, sf::Color color)
    {
        sf::Vector2f dir = b - a;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len <= 0.0f) return;
        sf::Vector2f n{-dir.y / len, dir.x / len};
        sf::Vector2f off = n * (thickness * 0.5f);

        sf::Vertex v0{a - off, color}; v0.texCoords = {0.f, 0.f};
        sf::Vertex v1{b - off, color}; v1.texCoords = {0.f, 0.f};
        sf::Vertex v2{b + off, color}; v2.texCoords = {0.f, 0.f};
        sf::Vertex v3{a + off, color}; v3.texCoords = {0.f, 0.f};

        lines.append(v0); lines.append(v1); lines.append(v2);
        lines.append(v0); lines.append(v2); lines.append(v3);
    };

    const sf::Color side_color(255, 255, 0);
    const sf::Color cap_color(255, 128, 0);

    const float y_bottom = static_cast<float>(0.0 - camera.y) * unit_size;
    const float y_top    = static_cast<float>(static_cast<double>(CHUNK_HEIGHT) - camera.y) * unit_size;

    for (int i = centerChunk - 2; i <= centerChunk + 3; ++i)
    {
        const float x = static_cast<float>(static_cast<double>(i * CHUNK_WIDTH) - camera.x) * unit_size;
        pushLine({x, y_bottom}, {x, y_top}, side_color);
    }

    for (int i = centerChunk - 2; i <= centerChunk + 2; ++i)
    {
        const float x_left  = static_cast<float>(static_cast<double>(i * CHUNK_WIDTH) - camera.x) * unit_size;
        const float x_right = static_cast<float>(static_cast<double>((i + 1) * CHUNK_WIDTH) - camera.x) * unit_size;

        pushLine({x_left, y_bottom}, {x_right, y_bottom}, cap_color);
        pushLine({x_left, y_top},    {x_right, y_top},    cap_color);
    }

    if (lines.getVertexCount() > 0) window.draw(lines);
}

// Mining-progress destruction overlay.
//
// Each cell is a block-local sub-pixel; the 3D-Perlin field is sampled in
// *world* coordinates so block (bx,by) always maps to the same slice of
// noise. Progress is quantised into STAGES discrete breaking steps so that
// the overlay mirrors the block's state: the first stage appears as soon
// as mining starts, and the final stage covers the whole block the frame
// before it breaks.
void RenderBlockDestruction(World& world, sf::Vector2<double> camera, sf::RenderWindow& window)
{
    static const PerlinNoise3D destruction_noise(0xB10CB17u);

    constexpr int    STAGES     = 10;
    constexpr int    RES        = 8;
    constexpr double NOISE_FREQ = 4.0;
    constexpr double NOISE_Z    = 0.5;

    const float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);
    const double pixel_world = 1.0 / static_cast<double>(RES);

    sf::VertexArray triangles(sf::PrimitiveType::Triangles);

    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<PlayerControlledComponent>()) continue;

        const auto& player = entity.getComponent<PlayerControlledComponent>();
        if (!player.mid_attack || player.mining_time <= 0.0f) continue;

        const sf::Vector2i bp = player.mining_block;
        const Block block = world.getBlock(bp.x, bp.y);
        if (block.id == BlockID::Air) continue;

        auto bd_it = blockDatabase.find(block.id);
        if (bd_it == blockDatabase.end()) continue;
        const BlockData& bd = bd_it->second;
        if (bd.hardness <= 0.0f) continue;

        const float progress = std::clamp(player.mining_time / bd.hardness, 0.0f, 1.0f);
        if (progress <= 0.0f) continue;

        const int stage = std::clamp(static_cast<int>(std::ceil(progress * STAGES)), 1, STAGES);
        const float effective = static_cast<float>(stage) / static_cast<float>(STAGES);

        // Clip to the block's visible footprint (water, doors, …).
        const sf::FloatRect bounds = bd.render.has_value()
            ? bd.render->render_bounds(block)
            : sf::FloatRect{{0.0f, 0.0f}, {1.0f, 1.0f}};
        if (bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) continue;

        for (int py = 0; py < RES; ++py)
        {
            for (int px = 0; px < RES; ++px)
            {
                const double lx = px * pixel_world;
                const double ly = py * pixel_world;

                if (lx + pixel_world <= bounds.position.x || lx >= bounds.position.x + bounds.size.x) continue;
                if (ly + pixel_world <= bounds.position.y || ly >= bounds.position.y + bounds.size.y) continue;

                const double nx = (static_cast<double>(bp.x) + lx + pixel_world * 0.5) * NOISE_FREQ;
                const double ny = (static_cast<double>(bp.y) + ly + pixel_world * 0.5) * NOISE_FREQ;
                const double n  = destruction_noise.noise(nx, ny, NOISE_Z);

                if (n >= effective) continue;

                const float wx0 = static_cast<float>(static_cast<double>(bp.x) + lx                 - camera.x) * unit_size;
                const float wx1 = static_cast<float>(static_cast<double>(bp.x) + lx + pixel_world   - camera.x) * unit_size;
                const float wy0 = static_cast<float>(static_cast<double>(bp.y) + ly                 - camera.y) * unit_size;
                const float wy1 = static_cast<float>(static_cast<double>(bp.y) + ly + pixel_world   - camera.y) * unit_size;

                const sf::Color c(0, 0, 0, 170);
                const sf::Vertex v0{{wx0, wy0}, c};
                const sf::Vertex v1{{wx1, wy0}, c};
                const sf::Vertex v2{{wx1, wy1}, c};
                const sf::Vertex v3{{wx0, wy1}, c};
                triangles.append(v0); triangles.append(v1); triangles.append(v2);
                triangles.append(v0); triangles.append(v2); triangles.append(v3);
            }
        }
    }

    if (triangles.getVertexCount() > 0)
    {
        window.draw(triangles);
    }
}

static float wrapToPi(float a)
{
    constexpr float PI = 3.14159265359f;
    while(a >  PI) a -= 2.0f * PI;
    while(a < -PI) a += 2.0f * PI;
    return a;
}


static void buildLitTriangles(World& world, sf::Vector2f source, sf::Vector2f cameraCenter, float halfViewW, float halfViewH, float maxDistance, unsigned int unit_size, sf::VertexArray& out)
{
    constexpr float PI = 3.14159265359f;
    constexpr float STEP_DEG = 0.02f;
    constexpr float STEP_RAD = STEP_DEG * PI / 180.0f;
    constexpr float MARGIN_RAD = 0.5f * PI / 180.0f;
    constexpr float EPS = 1e-9f;

    const float halfStepSin = std::sin(STEP_RAD * 0.5f);

    sf::Vector2f corners[4] =
    {
        {cameraCenter.x - halfViewW, cameraCenter.y - halfViewH},
        {cameraCenter.x + halfViewW, cameraCenter.y - halfViewH},
        {cameraCenter.x - halfViewW, cameraCenter.y + halfViewH},
        {cameraCenter.x + halfViewW, cameraCenter.y + halfViewH},
    };

    float refAngle = std::atan2(cameraCenter.y - source.y, cameraCenter.x - source.x);

    float dMin =  std::numeric_limits<float>::max();
    float dMax = -std::numeric_limits<float>::max();
    for(int i = 0; i < 4; ++i)
    {
        float a = std::atan2(corners[i].y - source.y, corners[i].x - source.x);
        float d = wrapToPi(a - refAngle);
        if(d < dMin) dMin = d;
        if(d > dMax) dMax = d;
    }

    float startAngle = refAngle + dMin - MARGIN_RAD;
    float endAngle   = refAngle + dMax + MARGIN_RAD;
    int numRays = static_cast<int>(std::ceil((endAngle - startAngle) / STEP_RAD));

    for(int i = 0; i < numRays; ++i)
    {
        float rad = startAngle + i * STEP_RAD;
        float dx = std::cos(rad);
        float dy = std::sin(rad);

        int bx = static_cast<int>(std::floor(source.x));
        int by = static_cast<int>(std::floor(source.y));

        int step_x = (dx > 0.0f) ? 1 : -1;
        int step_y = (dy > 0.0f) ? 1 : -1;

        float t_delta_x = (std::abs(dx) > EPS) ? std::abs(1.0f / dx) : std::numeric_limits<float>::max();
        float t_delta_y = (std::abs(dy) > EPS) ? std::abs(1.0f / dy) : std::numeric_limits<float>::max();

        float t_max_x;
        if(dx > EPS)        t_max_x = (std::floor(source.x) + 1.0f - source.x) / dx;
        else if(dx < -EPS)  t_max_x = (std::floor(source.x) - source.x) / dx;
        else                t_max_x = std::numeric_limits<float>::max();

        float t_max_y;
        if(dy > EPS)        t_max_y = (std::floor(source.y) + 1.0f - source.y) / dy;
        else if(dy < -EPS)  t_max_y = (std::floor(source.y) - source.y) / dy;
        else                t_max_y = std::numeric_limits<float>::max();

        float hit_t = maxDistance;
        float t = 0.0f;

        while(t < maxDistance)
        {
            if(t_max_x < t_max_y)
            {
                t = t_max_x;
                t_max_x += t_delta_x;
                bx += step_x;
            }
            else
            {
                t = t_max_y;
                t_max_y += t_delta_y;
                by += step_y;
            }

            if(t >= maxDistance) break;

            Block block = world.getBlock(bx, by);
            if(block.id != BlockID::Air && blockDatabase[block.id].transparent == false)
            {
                hit_t = t;
                break;
            }
        }

        constexpr float FADE_DISTANCE = 15.0f;

        sf::Vector2f innerBase{source.x + dx * hit_t, source.y + dy * hit_t};
        sf::Vector2f outerBase{source.x + dx * (hit_t + FADE_DISTANCE), source.y + dy * (hit_t + FADE_DISTANCE)};

        float halfWidthInner = hit_t * halfStepSin;
        float halfWidthOuter = (hit_t + FADE_DISTANCE) * halfStepSin;

        sf::Vector2f perpInner{-dy * halfWidthInner, dx * halfWidthInner};
        sf::Vector2f perpOuter{-dy * halfWidthOuter, dx * halfWidthOuter};

        sf::Color fullClear{0, 0, 0, 0};
        sf::Color noClear  {0, 0, 0, 215};

        sf::Vertex v_apex, v_inL, v_inR, v_outL, v_outR;

        v_apex.position = {source.x * unit_size, source.y * unit_size};
        v_inL.position  = {(innerBase.x + perpInner.x) * unit_size, (innerBase.y + perpInner.y) * unit_size};
        v_inR.position  = {(innerBase.x - perpInner.x) * unit_size, (innerBase.y - perpInner.y) * unit_size};
        v_outL.position = {(outerBase.x + perpOuter.x) * unit_size, (outerBase.y + perpOuter.y) * unit_size};
        v_outR.position = {(outerBase.x - perpOuter.x) * unit_size, (outerBase.y - perpOuter.y) * unit_size};

        v_apex.color = fullClear;
        v_inL.color  = fullClear;
        v_inR.color  = fullClear;
        v_outL.color = noClear;
        v_outR.color = noClear;

        out.append(v_apex);
        out.append(v_inL);
        out.append(v_inR);

        out.append(v_inL);
        out.append(v_inR);
        out.append(v_outR);

        out.append(v_inL);
        out.append(v_outR);
        out.append(v_outL);
    }
}

void RenderLightRays(World& world, sf::RenderWindow& window)
{
    static std::unique_ptr<sf::RenderTexture> shadowMap;
    static sf::Vector2u lastSize{0, 0};

    sf::Vector2u windowSize = window.getSize();

    if(!shadowMap || lastSize != windowSize)
    {
        shadowMap = std::make_unique<sf::RenderTexture>(windowSize);
        lastSize = windowSize;
    }

    unsigned int unit_size = windowSize.y / WORLD_UNIT_SIZE_FACTOR;

    sf::Vector2f cameraCenter
    {
        window.getView().getCenter().x / static_cast<float>(unit_size),
        window.getView().getCenter().y / static_cast<float>(unit_size)
    };

    sf::Vector2f viewSize = window.getView().getSize();
    float halfViewW = std::abs(viewSize.x) * 0.5f / static_cast<float>(unit_size);
    float halfViewH = std::abs(viewSize.y) * 0.5f / static_cast<float>(unit_size);

    sf::Vector2f sun = getSunWorldPosition(world, cameraCenter);

    shadowMap->clear(sf::Color(2, 4, 14, 215));
    shadowMap->setView(window.getView());

    sf::VertexArray litTriangles(sf::PrimitiveType::Triangles);
    buildLitTriangles(world, sun, cameraCenter, halfViewW, halfViewH, 600.0f, unit_size, litTriangles);

    sf::BlendMode minAlphaBlend(
        sf::BlendMode::Factor::Zero, sf::BlendMode::Factor::One, sf::BlendMode::Equation::Add,
        sf::BlendMode::Factor::One,  sf::BlendMode::Factor::One, sf::BlendMode::Equation::Min
    );

    sf::RenderStates states;
    states.blendMode = minAlphaBlend;
    shadowMap->draw(litTriangles, states);

    shadowMap->display();

    sf::View savedView = window.getView();
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, sf::Vector2f(windowSize))));

    sf::Sprite shadowSprite(shadowMap->getTexture());
    window.draw(shadowSprite);

    window.setView(savedView);
}

sf::Color lerpColor(sf::Color a, sf::Color b, float t)
{
    return sf::Color
    (
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t
    );
}

sf::Texture generateBackground()
{
    std::random_device rd;
    unsigned int seed = rd();
    std::mt19937 rng(seed);

    GenerationProperties props{};
    std::uniform_int_distribution<int> typeDist(0, 2);
    switch (typeDist(rng))
    {
        case 0:  props.type = GenerationType::Normal;         break;
        case 1:  props.type = GenerationType::Flat;           break;
        default: props.type = GenerationType::DungeonShooter; break;
    }

    std::uniform_real_distribution<float> baseDist (40.0f,    120.0f);
    std::uniform_real_distribution<float> scaleDist(10.0f,    40.0f);
    std::uniform_real_distribution<float> freqDist (0.02f,    0.08f);
    std::uniform_real_distribution<float> ampDist  (0.6f,     1.4f);
    std::uniform_real_distribution<float> persDist (0.3f,     0.7f);

    props.base_height  = baseDist(rng);
    props.height_scale = scaleDist(rng);
    props.frequency    = freqDist(rng);
    props.amplitude    = ampDist(rng);
    props.persistence  = persDist(rng);

    std::unique_ptr<WorldGenerator> generator;
    switch (props.type)
    {
        case GenerationType::Flat:
            generator = std::make_unique<FlatWorldGenerator>(FlatWorldProperties::defaults());
            break;
        case GenerationType::DungeonShooter:
            generator = std::make_unique<DungeonShooterWorldGenerator>(seed);
            break;
        case GenerationType::Skygrid:
            generator = std::make_unique<SkygridWorldGenerator>(seed);
            break;
        case GenerationType::Randomizer:
            generator = std::make_unique<RandomizerWorldGenerator>(seed);
            break;
        case GenerationType::Noodle:
            generator = std::make_unique<NoodleWorldGenerator>(seed, NormalWorldGeneratorProperties{
                props.base_height, props.height_scale, props.frequency, props.amplitude, props.persistence
            }, static_cast<double>(props.noodle_threshold));
            break;
        case GenerationType::Normal:
        default:
            generator = std::make_unique<NormalWorldGenerator>(seed, NormalWorldGeneratorProperties{
                props.base_height, props.height_scale, props.frequency, props.amplitude, props.persistence
            });
            break;
    }

    constexpr unsigned int OUT_W = 1920;
    constexpr unsigned int OUT_H = 1080;
    constexpr unsigned int UNIT_SIZE = 45;

    const float viewBlocksW = static_cast<float>(OUT_W) / UNIT_SIZE;
    const float viewBlocksH = static_cast<float>(OUT_H) / UNIT_SIZE;

    const float cameraX = 0.0f;

    const int firstCol = static_cast<int>(std::floor(cameraX - viewBlocksW * 0.5f)) - 2;
    const int lastCol  = static_cast<int>(std::ceil (cameraX + viewBlocksW * 0.5f)) + 2;

    auto chunkOf = [](int world_x) -> int
    {
        return static_cast<int>(std::floor(static_cast<float>(world_x) / static_cast<float>(CHUNK_WIDTH)));
    };

    std::unordered_map<int, Chunk> chunks;
    for (int wx = firstCol; wx <= lastCol; ++wx)
    {
        const int cp = chunkOf(wx);
        if (chunks.find(cp) == chunks.end())
        {
            chunks.emplace(cp, generator->generateChunk(cp));
        }
    }

    auto getBlock = [&](int wx, int wy) -> Block
    {
        if (wy < 0 || wy >= CHUNK_HEIGHT) return {BlockID::Air, 0};
        const int cp = chunkOf(wx);
        auto it = chunks.find(cp);
        if (it == chunks.end()) return {BlockID::Air, 0};
        return it->second.blocks[wy][wx - cp * CHUNK_WIDTH];
    };

    float cameraY;
    if (props.type == GenerationType::DungeonShooter)
    {
        const float lo = 8.0f;
        const float hi = std::max(lo + 1.0f, static_cast<float>(CHUNK_HEIGHT) - viewBlocksH - 8.0f);
        std::uniform_real_distribution<float> bottomDist(lo, hi);
        cameraY = bottomDist(rng) + viewBlocksH * 0.5f;
    }
    else
    {
        int minTop = CHUNK_HEIGHT;
        bool found = false;
        for (int wx = firstCol; wx <= lastCol; ++wx)
        {
            for (int wy = CHUNK_HEIGHT - 1; wy >= 0; --wy)
            {
                if (getBlock(wx, wy).id != BlockID::Air)
                {
                    minTop = std::min(minTop, wy);
                    found = true;
                    break;
                }
            }
        }
        if (!found) minTop = 0;
        const float bottomView = static_cast<float>(minTop) - 2.0f;
        cameraY = bottomView + viewBlocksH * 0.5f;
    }

    sf::RenderTexture rt(sf::Vector2u{OUT_W, OUT_H});
    rt.clear();

    constexpr sf::Color skyTop   {80, 160, 240};
    constexpr sf::Color skyBottom{170, 215, 255};

    sf::VertexArray sky(sf::PrimitiveType::TriangleStrip, 4);
    sky[0].position = {0.0f, 0.0f};
    sky[1].position = {static_cast<float>(OUT_W), 0.0f};
    sky[2].position = {0.0f, static_cast<float>(OUT_H)};
    sky[3].position = {static_cast<float>(OUT_W), static_cast<float>(OUT_H)};
    sky[0].color = skyTop;
    sky[1].color = skyTop;
    sky[2].color = skyBottom;
    sky[3].color = skyBottom;
    rt.draw(sky);

    const int minY = static_cast<int>(std::floor(cameraY - viewBlocksH * 0.5f));
    const int maxY = static_cast<int>(std::ceil (cameraY + viewBlocksH * 0.5f));

    for (int wx = firstCol; wx <= lastCol; ++wx)
    {
        const float px = (static_cast<float>(wx) - cameraX + viewBlocksW * 0.5f) * UNIT_SIZE;

        for (int wy = minY; wy <= maxY; ++wy)
        {
            const Block block = getBlock(wx, wy);
            if (block.id == BlockID::Air) continue;

            const auto& data = blockDatabase[block.id];
            if (!data.render.has_value()) continue;

            const BlockNeighbors neighbors{
                getBlock(wx,     wy + 1).id,
                getBlock(wx + 1, wy    ).id,
                getBlock(wx,     wy - 1).id,
                getBlock(wx - 1, wy    ).id
            };
            const sf::IntRect uv = data.render->rect(block, neighbors);
            if (uv.size.x <= 0 || uv.size.y <= 0) continue;

            sf::Sprite sprite(AssetManager::getGameTexture(data.render->texture));
            sprite.setTextureRect(uv);

            const float py = (cameraY + viewBlocksH * 0.5f - static_cast<float>(wy + 1)) * UNIT_SIZE;
            sprite.setPosition({px, py});
            sprite.setScale({
                static_cast<float>(UNIT_SIZE) / static_cast<float>(uv.size.x),
                static_cast<float>(UNIT_SIZE) / static_cast<float>(uv.size.y)
            });

            rt.draw(sprite);
        }
    }

    rt.display();
    return sf::Texture(rt.getTexture());
}

void renderUIBackground(sf::FloatRect bounds, sf::RenderTarget& target)
{
        
}

sf::Vector2i getMouseBlockPosition(sf::Vector2<double> camera, const sf::RenderWindow& window)
{
    auto world_position = getMouseWorldPosition(camera, window);

    return {static_cast<int>(std::floor(world_position.x)), static_cast<int>(std::floor(world_position.y))};
}

sf::Vector2<double> getMouseWorldPosition(sf::Vector2<double> camera, const sf::RenderWindow& window)
{
    double unit_size = static_cast<double>(window.getSize().y) / static_cast<double>(WORLD_UNIT_SIZE_FACTOR);

    sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    return camera + sf::Vector2<double>{(mouseWorld.x / unit_size), (mouseWorld.y / unit_size)};
}


struct Star
{
    sf::Vector2f position;
    float size;
    float brightness;
    float twinkleSpeed;
    float twinklePhase;
};

void renderStars(float phase, sf::RenderWindow& window)
{
    static std::vector<Star> stars;
    static sf::Vector2u window_size_generated;
    static sf::Clock twinkleClock;

    const float W = static_cast<float>(window.getSize().x);
    const float H = static_cast<float>(window.getSize().y);

    if (stars.empty() || window_size_generated != window.getSize())
    {
        stars.clear();

        window_size_generated = window.getSize();

        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> distX(0.0f, W);
        std::uniform_real_distribution<float> distY(0.0f, H);
        std::uniform_int_distribution<int> distSize(2, 4);
        std::uniform_real_distribution<float> distBright(0.4f, 1.0f);
        std::uniform_real_distribution<float> distSpeed(0.5f, 2.5f);
        std::uniform_real_distribution<float> distPhase(0.0f, 6.2831853f);

        constexpr int STAR_COUNT = 250;

        stars.reserve(STAR_COUNT);

        for (int i = 0; i < STAR_COUNT; ++i)
        {
            stars.push_back({{std::floor(distX(rng)), std::floor(distY(rng))}, static_cast<float>(distSize(rng)), distBright(rng), distSpeed(rng), distPhase(rng)});
        }
    }

    float sun_angle = phase * 2.0f * static_cast<float>(std::numbers::pi);
    float sunHeight = -std::cos(sun_angle);

    float nightFactor = std::clamp(-sunHeight * 1.6f, 0.f, 1.f);

    if (nightFactor <= 0.001f) return;

    float t = twinkleClock.getElapsedTime().asSeconds();

    sf::VertexArray quads(sf::PrimitiveType::Triangles, stars.size() * 6);
    std::size_t vi = 0;

    for (const Star& s : stars)
    {
        float twinkle = 0.5f
            + 0.35f * std::sin(t * s.twinkleSpeed + s.twinklePhase)
            + 0.15f * std::sin(t * s.twinkleSpeed * 2.7f + s.twinklePhase * 1.3f);

        twinkle = std::clamp(twinkle, 0.0f, 1.0f);

        auto alpha = static_cast<std::uint8_t>(255.f * s.brightness * twinkle * nightFactor);
        sf::Color c(255, 255, 255, alpha);

        float x0 = s.position.x;
        float y0 = s.position.y;
        float x1 = x0 + s.size;
        float y1 = y0 + s.size;

        quads[vi+0] = {{x0, y0}, c};
        quads[vi+1] = {{x1, y0}, c};
        quads[vi+2] = {{x1, y1}, c};
        quads[vi+3] = {{x0, y0}, c};
        quads[vi+4] = {{x1, y1}, c};
        quads[vi+5] = {{x0, y1}, c};

        vi += 6;
    }

    sf::View previous = window.getView();
    window.setView(sf::View(sf::FloatRect({0.f, 0.f}, {W, H})));

    sf::RenderStates states;
    states.blendMode = sf::BlendAdd;
    window.draw(quads, states);

    window.setView(previous);
}

void NetworkInterpolationSystem(World& world, double render_tick)
{
    // Limits how far past history.back().tick we're willing to extrapolate.
    // Expressed in ticks. Beyond this we hold the last sample — the alternative
    // is a hard overshoot whenever an entity decelerates while samples are in
    // flight. One full interpolation window's worth of ticks is enough to
    // cover a single dropped-then-retransmitted snapshot without a visible
    // freeze and without flying off.
    constexpr double MAX_EXTRAPOLATION_TICKS = 6.0;

    for (auto& [id, entity] : world.getEntities())
    {
        if (!entity.hasComponent<NetworkInterpolationComponent>() || !entity.hasComponent<TransformComponent>()) continue;

        auto& history = entity.getComponent<NetworkInterpolationComponent>().history;
        auto& transform = entity.getComponent<TransformComponent>();

        if (history.empty()) continue;

        if (history.size() == 1)
        {
            transform.render_position = history.front().position;
            continue;
        }

        if (render_tick <= static_cast<double>(history.front().tick))
        {
            transform.render_position = history.front().position;
            continue;
        }

        if (render_tick >= static_cast<double>(history.back().tick))
        {
            // No sample newer than render_tick yet — extrapolate briefly from
            // the last two samples so the entity keeps moving instead of
            // freezing. Bounded by MAX_EXTRAPOLATION_TICKS; past that we hold
            // the last sample rather than flying off.
            //
            // For player-controlled entities we additionally consult the
            // server-sent move_intent: if intent.x == 0 the remote player
            // explicitly stopped, so extrapolating from the last position
            // delta would overshoot the stop point and snap back when the
            // next real sample arrives. In that case we hold the last
            // sample. If intent.x has a sign we also clamp the extrapolation
            // direction to that sign — handles cases where the two
            // snapshots we're using straddled a direction change.
            const auto& a = history[history.size() - 2];
            const auto& b = history.back();
            const double span = static_cast<double>(b.tick) - static_cast<double>(a.tick);

            bool hold_last = false;
            double dir_clamp = 0.0; // 0 = no clamp, +1 / -1 = clamp to that sign
            if (entity.hasComponent<PlayerControlledComponent>())
            {
                const auto& player = entity.getComponent<PlayerControlledComponent>();
                if (std::abs(player.move_intent.x) < 0.01f)
                {
                    hold_last = true;
                }
                else
                {
                    dir_clamp = (player.move_intent.x > 0.0f) ? 1.0 : -1.0;
                }
            }

            if (hold_last || span <= 0.0)
            {
                transform.render_position = b.position;
            }
            else
            {
                const double raw_overshoot = render_tick - static_cast<double>(b.tick);
                const double overshoot = std::min(raw_overshoot, MAX_EXTRAPOLATION_TICKS);
                const double alpha = 1.0 + overshoot / span;
                sf::Vector2<double> predicted = a.position + (b.position - a.position) * alpha;
                if (dir_clamp != 0.0)
                {
                    const double delta_x = predicted.x - b.position.x;
                    if ((dir_clamp > 0.0 && delta_x < 0.0) ||
                        (dir_clamp < 0.0 && delta_x > 0.0))
                    {
                        predicted.x = b.position.x;
                    }
                }
                transform.render_position = predicted;
            }
            continue;
        }

        const NetworkInterpolationComponent::Sample* before = &history.front();
        const NetworkInterpolationComponent::Sample* after  = &history.back();

        for (std::size_t i = 0; i + 1 < history.size(); ++i)
        {
            if (static_cast<double>(history[i].tick) <= render_tick && render_tick <= static_cast<double>(history[i + 1].tick))
            {
                before = &history[i];
                after  = &history[i + 1];
                break;
            }
        }

        double span  = static_cast<double>(after->tick) - static_cast<double>(before->tick);
        double alpha = span > 0.0
            ? std::clamp((render_tick - static_cast<double>(before->tick)) / span, 0.0, 1.0)
            : 1.0;

        transform.render_position = before->position + (after->position - before->position) * alpha;
    }
}

sf::Color getRarityColor(ItemRarity rarity)
{
    switch(rarity)
    {
    case ItemRarity::Common:

        return sf::Color::White;

    case ItemRarity::Rare:

        return sf::Color::Green;

    case ItemRarity::Super_Rare:

        return sf::Color::Blue;

    case ItemRarity::Epic:

        return sf::Color(128, 0, 128);

    case ItemRarity::Mythic:

        return sf::Color::Red;
    }

    return sf::Color::Black;
}


constexpr int TEXT_SIZE = 20;

namespace
{
    std::string enchantName(EnchantID id)
    {
        switch (id)
        {
        case EnchantID::Sharpness:  return "Sharpness";
        case EnchantID::Efficiency: return "Efficiency";
        case EnchantID::Unbreaking: return "Unbreaking";
        case EnchantID::Fortune:    return "Fortune";
        case EnchantID::None:       break;
        }
        return "Unknown";
    }

    std::string romanNumeral(uint8_t level)
    {
        static const char* table[] = {
            "", "I", "II", "III", "IV", "V",
            "VI", "VII", "VIII", "IX", "X"
        };
        if (level < sizeof(table) / sizeof(table[0]))
            return table[level];
        return std::to_string(level);
    }
}

void renderItemInfo(sf::Vector2f position, const ItemStack& item, sf::RenderTarget& target)
{
    const auto& item_data = itemDatabase[item.itemID];

    struct Line
    {
        std::string text;
        sf::Color   color;
    };

    std::vector<Line> lines;
    lines.push_back({getDisplayName(item), getRarityColor(item_data.rarity)});

    if (item_data.maxDurability.has_value())
    {
        uint32_t max_dur = *item_data.maxDurability;
        uint32_t cur_dur = getCurrentDurability(item);

        float ratio = max_dur == 0 ? 1.0f :
            static_cast<float>(cur_dur) / static_cast<float>(max_dur);

        sf::Color bar_color = sf::Color::Green;
        if (ratio < 0.33f)      bar_color = sf::Color::Red;
        else if (ratio < 0.66f) bar_color = sf::Color(255, 165, 0);

        lines.push_back({
            "Durability: " + std::to_string(cur_dur) + " / " + std::to_string(max_dur),
            bar_color
        });
    }

    if (item.instanceID.has_value())
    {
        if (const ItemInstanceData* data = findInstance(*item.instanceID))
        {
            if (const EnchantmentsItemComponent* ench = data->get<EnchantmentsItemComponent>())
            {
                for (const Enchantment& e : ench->entries)
                {
                    if (e.id == EnchantID::None) continue;
                    lines.push_back({
                        enchantName(e.id) + " " + romanNumeral(e.level),
                        sf::Color(170, 170, 255)
                    });
                }
            }

            if (const MagicEffectItemComponent* bottle = data->get<MagicEffectItemComponent>())
            {
                if (bottle->effect != EffectID::None && EffectRegistry::contains(bottle->effect))
                {
                    const auto& def = EffectRegistry::get(bottle->effect);

                    std::string effect_line = def.display_name.empty()
                        ? std::string("Effect ") + std::to_string(static_cast<uint32_t>(bottle->effect))
                        : def.display_name;
                    if (bottle->magnitude > 1)
                    {
                        effect_line += " ";
                        effect_line += romanNumeral(static_cast<uint8_t>(bottle->magnitude));
                    }
                    lines.push_back({effect_line, def.liquid_color});

                    int seconds = static_cast<int>(bottle->duration + 0.5f);
                    lines.push_back({
                        std::to_string(seconds) + "s",
                        sf::Color(200, 200, 200)
                    });
                }
            }
        }
    }

    if (isRubyTool(item.itemID))
    {
        uint8_t level = getRubynessLevel(item);
        std::string letter = greekLetterName(level);
        if (!letter.empty())
        {
            letter[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(letter[0])));
        }
        lines.push_back({
            "Rubyness: " + letter,
            sf::Color(220, 60, 90)
        });
    }

    lines.push_back({
        "ID: " + std::to_string(static_cast<uint32_t>(item.itemID)),
        sf::Color(160, 160, 160)
    });

    auto& font = AssetManager::getFont(AssetManager::FontID::PressStart2P);

    std::vector<sf::Text> texts;
    texts.reserve(lines.size());
    for (const Line& l : lines)
    {
        sf::Text t(font, l.text, TEXT_SIZE);
        t.setFillColor(l.color);
        texts.push_back(std::move(t));
    }

    float padding     = static_cast<float>(TEXT_SIZE) * 0.4f;
    float lineSpacing = static_cast<float>(TEXT_SIZE) * 0.3f;

    float max_width = 0.0f;
    float total_height = 0.0f;
    std::vector<float> line_heights;
    line_heights.reserve(texts.size());

    for (const sf::Text& t : texts)
    {
        sf::FloatRect b = t.getLocalBounds();
        max_width = std::max(max_width, b.size.x);
        line_heights.push_back(b.size.y);
        total_height += b.size.y;
    }
    total_height += lineSpacing * static_cast<float>(texts.size() > 0 ? texts.size() - 1 : 0);

    float frameWidth  = max_width + padding * 2.0f;
    float frameHeight = total_height + padding * 2.0f;

    sf::Vector2f framePos = position + sf::Vector2f(15.0f, 15.0f);
    sf::Vector2u windowSize = target.getSize();

    if (framePos.x + frameWidth > windowSize.x)  framePos.x = windowSize.x - frameWidth;
    if (framePos.y + frameHeight > windowSize.y) framePos.y = windowSize.y - frameHeight;

    sf::RectangleShape frame({frameWidth, frameHeight});
    frame.setPosition(framePos);
    frame.setFillColor(sf::Color(0, 0, 0, 200));
    frame.setOutlineColor(sf::Color::White);
    frame.setOutlineThickness(1.0f);
    target.draw(frame);

    float y_cursor = framePos.y + padding;
    for (size_t i = 0; i < texts.size(); ++i)
    {
        texts[i].setPosition({framePos.x + padding, y_cursor});
        target.draw(texts[i]);
        y_cursor += line_heights[i] + lineSpacing;
    }
}

void renderBar(int max, int value, sf::Color primary_color, sf::Color secondary_color, sf::FloatRect bounds, sf::RenderTarget& target)
{
    sf::RectangleShape background(bounds.size);
    background.setPosition(bounds.position);
    background.setFillColor(sf::Color(72, 72, 72, 125));

    target.draw(background);

    const float factor = std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.0f, 1.0f);

    static const PerlinNoise3D bar_perlin(1337u);
    static sf::Clock bar_clock;
    const float t = bar_clock.getElapsedTime().asSeconds();

    if (factor > 0.0f)
    {
        const float filled_width = bounds.size.x * factor;

        const float cell_px = std::max(3.0f, bounds.size.y * 0.22f);

        const float leading_edge = std::min(filled_width, cell_px * 1.5f);
        const float noise_width = filled_width - leading_edge;

        auto clip = [](float c) {
            return static_cast<std::uint8_t>(std::clamp(c, 0.0f, 255.0f));
        };

        auto sample = [&](float u, float v) {
            const sf::Color base = lerpColor(secondary_color, primary_color, u);

            const double n1 = bar_perlin.noise(u * 10.0, v * 6.0, t * 0.8);
            const double n2 = bar_perlin.noise(u * 24.0 + 11.3, v * 14.0, t * 1.8 + 7.7);
            const double n3 = bar_perlin.noise(u * 55.0 + 3.1, v * 28.0, t * 2.6 + 2.2);
            const float n_raw = static_cast<float>(n1 * 0.5 + n2 * 0.32 + n3 * 0.18);

            const float sign = n_raw < 0.0f ? -1.0f : 1.0f;
            const float n = sign * std::pow(std::abs(n_raw), 0.4f);

            const float brightness = 0.55f + n * 1.15f;

            return sf::Color(
                clip(base.r * brightness),
                clip(base.g * brightness),
                clip(base.b * brightness),
                base.a
            );
        };

        if (noise_width > 0.0f)
        {
            const int cols = std::max(1, static_cast<int>(noise_width / cell_px));
            const int rows = std::max(2, static_cast<int>(bounds.size.y / cell_px));

            sf::VertexArray cells(sf::PrimitiveType::Triangles, static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows) * 6);

            for (int row = 0; row < rows; ++row)
            {
                for (int col = 0; col < cols; ++col)
                {
                    const float u = (static_cast<float>(col) + 0.5f) / static_cast<float>(cols);
                    const float v = (static_cast<float>(row) + 0.5f) / static_cast<float>(rows);
                    const sf::Color c = sample(u, v);

                    const float x0 = bounds.position.x + noise_width * static_cast<float>(col)     / static_cast<float>(cols);
                    const float x1 = bounds.position.x + noise_width * static_cast<float>(col + 1) / static_cast<float>(cols);
                    const float y0 = bounds.position.y + bounds.size.y * static_cast<float>(row)     / static_cast<float>(rows);
                    const float y1 = bounds.position.y + bounds.size.y * static_cast<float>(row + 1) / static_cast<float>(rows);

                    const std::size_t i = (static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col)) * 6;
                    cells[i + 0] = sf::Vertex{{x0, y0}, c};
                    cells[i + 1] = sf::Vertex{{x1, y0}, c};
                    cells[i + 2] = sf::Vertex{{x1, y1}, c};
                    cells[i + 3] = sf::Vertex{{x0, y0}, c};
                    cells[i + 4] = sf::Vertex{{x1, y1}, c};
                    cells[i + 5] = sf::Vertex{{x0, y1}, c};
                }
            }

            target.draw(cells);
        }

        sf::RectangleShape leading({leading_edge, bounds.size.y});
        leading.setPosition({bounds.position.x + noise_width, bounds.position.y});
        leading.setFillColor(primary_color);
        target.draw(leading);
    }

    const float frame_thickness = std::max(1.0f, bounds.size.y * 0.06f);
    sf::RectangleShape frame(bounds.size);
    frame.setPosition(bounds.position);
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineColor(sf::Color(0, 0, 0, 220));
    frame.setOutlineThickness(-frame_thickness);
    target.draw(frame);

    const unsigned int character_size = std::max(8u, static_cast<unsigned int>(bounds.size.y * 0.4f));
    sf::Text value_text(AssetManager::getFont(AssetManager::FontID::PressStart2P),
                        std::to_string(value),
                        character_size);
    value_text.setFillColor(sf::Color::White);

    const sf::FloatRect text_bounds = value_text.getLocalBounds();
    value_text.setOrigin({text_bounds.position.x + text_bounds.size.x * 0.5f,
                          text_bounds.position.y + text_bounds.size.y * 0.5f});
    value_text.setPosition(bounds.position + bounds.size * 0.5f);
    target.draw(value_text);
}

void renderHotbar(std::array<ItemStack, 9> items, uint8_t selected_slot, sf::RenderTarget& target)
{
    sf::Vector2f size = {static_cast<float>(target.getSize().x) * 0.5f, static_cast<float>(target.getSize().x) * 0.5f * (13.0f / 101.0f)};
    sf::Vector2f position = {(static_cast<float>(target.getSize().x) - size.x) / 2.f, static_cast<float>(target.getSize().y) * 0.8f};

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setTexture(&AssetManager::getUITexture(AssetManager::UITextureID::Hotbar));
    target.draw(background);

    float slotWidth = size.x * (10.f / 101.f);
    float slotSpacing = size.x * (2.f / 202.f);
    float slotStart = size.x * (3.f / 202.f);
    float verticalOffset = size.y * (3.f / 26.f);

    for(int i = 0; i < 9; i++)
    {
        ItemStack& stack = items[i];

        Slot slot(position + sf::Vector2f(slotStart + (slotWidth + slotSpacing) * i, verticalOffset), {slotWidth, slotWidth});

        slot.setItemStack(stack);
        slot.setHovered(i == selected_slot);
        slot.render(target);
    }
}

void RenderMinimap(ClientWorld& world, sf::Vector2<double> camera, sf::RenderTarget& target)
{
    constexpr unsigned int MAX_MINIMAP_TEXTURE_WIDTH = 512;

    const float minimap_scale  = std::clamp(Settings::minimap.size, 0.02f, 1.0f);
    const float minimap_pixels = static_cast<float>(target.getSize().x) * minimap_scale;

    unsigned int tex_dim = std::min(static_cast<unsigned int>(std::max(minimap_pixels, 1.0f)), MAX_MINIMAP_TEXTURE_WIDTH);
    sf::Vector2u texture_size = {tex_dim, tex_dim};

    sf::RenderTexture render_texture(texture_size);
    render_texture.clear(sf::Color::Transparent);

    sf::View view({0.0f, 0.0f}, {static_cast<float>(texture_size.x), static_cast<float>(texture_size.y)});
    view.setSize({view.getSize().x, -(view.getSize().y)});
    render_texture.setView(view);

    auto [sky_top, sky_bottom] = getSkyGradient(solarPhase(world.getDayTime(), world.days));
    renderSky(render_texture, sky_top, sky_bottom);

    const float day_factor      = computeDayFactor(solarPhase(world.getDayTime(), world.days));
    constexpr float ambient_min     = 0.0f;
    constexpr float night_sky_floor = 4.0f / 15.0f;
    const float effect_strength = std::clamp(Settings::graphics.lighting_effect_strength, 0.0f, 1.0f);

    float unit_size = static_cast<float>(texture_size.x) / 80.0f;

    const bool flat_colors = Settings::minimap.flat_colors;

    for(int i = positionToChunk(camera) - 5; i <= positionToChunk(camera) + 5; i++)
    {
        Chunk& chunk = world.getChunk(i);
        ChunkMesh& mesh = world.getChunkMeshes()[i];

        if(!chunk.generated || !mesh.built) continue;

        float translate_x = static_cast<float>((static_cast<double>(i * CHUNK_WIDTH) - camera.x)) * unit_size;
        float translate_y = static_cast<float>(-camera.y) * unit_size;

        sf::Transform block_transform;
        block_transform.translate({translate_x, translate_y});
        block_transform.scale({unit_size, unit_size});

        if(flat_colors)
        {
            sf::VertexArray flat_vertices{sf::PrimitiveType::Triangles};

            for(int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                for(int lx = 0; lx < CHUNK_WIDTH; ++lx)
                {
                    const Block& b = chunk.blocks[y][lx];
                    if(b.id == BlockID::Air) continue;
                    const auto& data = blockDatabase[b.id];
                    if(!data.render.has_value()) continue;

                    sf::Color color = BlockAtlas::getAverageColor(data.render->texture);
                    if(color.a == 0) continue;
                    color.a = 255;

                    float x0 = translate_x + static_cast<float>(lx) * unit_size;
                    float y0 = translate_y + static_cast<float>(y)  * unit_size;
                    float x1 = x0 + unit_size;
                    float y1 = y0 + unit_size;

                    sf::Vertex v0; v0.position = {x0, y0}; v0.color = color;
                    sf::Vertex v1; v1.position = {x1, y0}; v1.color = color;
                    sf::Vertex v2; v2.position = {x1, y1}; v2.color = color;
                    sf::Vertex v3; v3.position = {x0, y1}; v3.color = color;

                    flat_vertices.append(v0);
                    flat_vertices.append(v1);
                    flat_vertices.append(v2);
                    flat_vertices.append(v0);
                    flat_vertices.append(v2);
                    flat_vertices.append(v3);
                }
            }

            if(flat_vertices.getVertexCount() > 0) render_texture.draw(flat_vertices);
        }
        else
        {
            sf::RenderStates states;

            states.texture   = &BlockAtlas::getTexture();
            states.transform = block_transform;
            applyBlockAnimationShader(states);

            render_texture.draw(mesh.vertices, states);
        }

        if(Settings::minimap.render_shadows && chunk.light.initialized)
        {
            sf::Texture& light_tex = ensureLightTexture(world, i);
            if (chunk.light.texture_dirty)
            {
                auto pixels = packChunkLightmap(chunk.light);
                light_tex.update(pixels.data());
                chunk.light.texture_dirty = false;
            }

            sf::Shader& lighting_shader = AssetManager::getShader(AssetManager::ShaderID::LightingOverlay);
            lighting_shader.setUniform("texture", sf::Shader::CurrentTexture);
            lighting_shader.setUniform("u_day_factor",      day_factor);
            lighting_shader.setUniform("u_ambient_min",     ambient_min);
            lighting_shader.setUniform("u_night_sky_floor", night_sky_floor);
            lighting_shader.setUniform("u_effect_strength", effect_strength);

            sf::RenderStates overlay_states;
            overlay_states.shader    = &lighting_shader;
            overlay_states.texture   = &light_tex;
            overlay_states.transform = block_transform;
            overlay_states.blendMode = sf::BlendMultiply;

            render_texture.draw(mesh.lightmap_vertices, overlay_states);
        }
    }

    render_texture.display();


    sf::RectangleShape minimap({minimap_pixels, minimap_pixels});

    minimap.setTexture(&render_texture.getTexture());

    minimap.setPosition({Settings::minimap.offset_x, Settings::minimap.offset_y});

    minimap.setOutlineThickness(5.0f);
    minimap.setOutlineColor(sf::Color(72, 72, 72));

    target.draw(minimap);
}