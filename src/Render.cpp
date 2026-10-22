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

#include <memory>
#include <random>
#include <numbers>
#include <unordered_map>

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
}

void BuildChunkMesh(ClientWorld& world, int chunk_position)
{
    Chunk& chunk = world.getChunk(chunk_position);
    ChunkMesh& mesh = world.getChunkMeshes()[chunk_position];

    mesh.vertices.clear();

    for (int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for (int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block& block = chunk.blocks[y][x];

            auto block_data = blockDatabase[block.id];

            if(!block_data.render.has_value()) continue;

            auto texture_id = block_data.render->texture;

            const sf::IntRect& texture_uv = BlockAtlas::getUV(texture_id);

            sf::IntRect final_uv = {texture_uv.position + block_data.render->rect(block).position, block_data.render->rect(block).size};

            sf::Vector2f position(x, y);

            auto bounds = block_data.render->render_bounds(block);

            sf::FloatRect final_rect = {position + bounds.position, bounds.size};

            // Static blocks encode frame_count=1 so the shader short-circuits
            // and samples the texCoord unchanged.
            const BlockAnimation anim = block_data.render->animation(block);
            const uint16_t duration_cs = anim.frame_duration_ms / 10u;
            sf::Color anim_color{
                anim.frame_count,
                static_cast<uint8_t>(std::min<int>(final_uv.size.x, 255)),
                static_cast<uint8_t>(std::min<uint16_t>(duration_cs, 255)),
                255
            };

            appendQuad(mesh.vertices, final_rect, final_uv, anim_color);
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

void renderSunAndMoon(float daytime, sf::RenderWindow& window)
{
    sf::Shader& shader = AssetManager::getShader(AssetManager::ShaderID::Sun);

    float passed = daytime / World::DAY_CYCLE_DURATION;

    float sun_angle  = passed * 2.0f * static_cast<float>(std::numbers::pi);
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

    float t = std::fmod(world.getDayTime() / World::DAY_CYCLE_DURATION, 1.0f);

    float normalized = std::clamp((t - 0.25f) / 0.5f, 0.0f, 1.0f);
    float angle = normalized * PI;

    return
    {
        cameraCenter.x - std::cos(angle) * SUN_DISTANCE,
        cameraCenter.y + std::sin(angle) * SUN_DISTANCE
    };
}

void RenderWorld(ClientWorld& world, const sf::Vector2<double> camera, sf::RenderWindow& window)
{
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

        float translate_x = static_cast<float>((static_cast<double>(i * CHUNK_WIDTH) - camera.x)) * unit_size;
        float translate_y = static_cast<float>(-camera.y) * unit_size;

        sf::RenderStates states;
        states.texture = &BlockAtlas::getTexture();
        states.transform.translate({translate_x, translate_y});
        states.transform.scale({unit_size, unit_size});
        applyBlockAnimationShader(states);

        window.draw(mesh.vertices, states);
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

void RenderBlockOutline(World& world, const sf::Vector2<double> camera, const sf::Vector2i block, sf::RenderWindow& window)
{
    constexpr float TRANSITION_DURATION = 0.1f;
    constexpr float INSET = 1.0f / 16.0f;

    auto resolveBounds = [&]() -> sf::FloatRect
    {
        Block b = world.getBlock(block.x, block.y);
        auto it = blockDatabase.find(b.id);
        if (it != blockDatabase.end() && it->second.render.has_value())
        {
            sf::FloatRect r = it->second.render->render_bounds(b);
            if (r.size.x > 0.0f && r.size.y > 0.0f) return r;
        }
        return {{0.0f, 0.0f}, {1.0f, 1.0f}};
    };

    sf::FloatRect local = resolveBounds();
    sf::FloatRect target
    {
        {static_cast<float>(block.x) + local.position.x, static_cast<float>(block.y) + local.position.y},
        local.size
    };

    static sf::FloatRect source_rect{};
    static sf::FloatRect target_rect{};
    static sf::FloatRect current_rect{};
    static float transition_elapsed = TRANSITION_DURATION;
    static bool initialized = false;
    static sf::Clock clock;

    const float dt = clock.restart().asSeconds();

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
        float t = transition_elapsed / TRANSITION_DURATION;
        t = t * t * (3.0f - 2.0f * t);

        current_rect.position.x = source_rect.position.x + (target_rect.position.x - source_rect.position.x) * t;
        current_rect.position.y = source_rect.position.y + (target_rect.position.y - source_rect.position.y) * t;
        current_rect.size.x     = source_rect.size.x     + (target_rect.size.x     - source_rect.size.x)     * t;
        current_rect.size.y     = source_rect.size.y     + (target_rect.size.y     - source_rect.size.y)     * t;
    }
    else
    {
        current_rect = target_rect;
    }

    float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    float inset = std::min(INSET, 0.5f * std::min(current_rect.size.x, current_rect.size.y));

    sf::RectangleShape shape(
        {
            (current_rect.size.x - 2.0f * inset) * unit_size,
            (current_rect.size.y - 2.0f * inset) * unit_size
        }
    );

    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color::Black);
    shape.setOutlineThickness(inset * unit_size);

    shape.setPosition(
        {
            (static_cast<float>(static_cast<double>(current_rect.position.x) - camera.x) + inset) * unit_size,
            (static_cast<float>(static_cast<double>(current_rect.position.y) - camera.y) + inset) * unit_size
        }
    );

    window.draw(shape);
}

// Mining-progress destruction overlay.
//
// Each cell is a block-local sub-pixel; the 3D-Perlin field is sampled in
// *world* coordinates so block (bx,by) always maps to the same slice of
// noise. Progress drives the threshold: as it rises past ENTRY_THRESHOLD,
// more and more pixels fall under it and get painted black. The sampling
// grid gives the characteristic "cracked pixel" look.
void RenderBlockDestruction(World& world, sf::Vector2<double> camera, sf::RenderWindow& window)
{
    static const PerlinNoise3D destruction_noise(0xB10CB17u);

    constexpr float  ENTRY_THRESHOLD = 0.15f;
    constexpr int    RES             = 8;
    constexpr double NOISE_FREQ      = 4.0;
    constexpr double NOISE_Z         = 0.5;

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
        if (progress < ENTRY_THRESHOLD) continue;

        const float effective = (progress - ENTRY_THRESHOLD) / (1.0f - ENTRY_THRESHOLD);

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

            const sf::IntRect uv = data.render->rect(block);
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

void renderStars(float daytime, sf::RenderWindow& window)
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

    float passed  = daytime / World::DAY_CYCLE_DURATION;
    float sun_angle = passed * 2.0f * static_cast<float>(std::numbers::pi);
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

void NetworkInterpolationSystem(World& world, uint64_t latest_tick, float tick_step, float interpolation_delay_seconds)
{
    double render_tick = static_cast<double>(latest_tick) - (interpolation_delay_seconds / tick_step);

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
            transform.render_position = history.back().position;
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

void renderItemInfo(sf::Vector2f position, const ItemStack& item, sf::RenderTarget& target)
{
    const auto& item_data = itemDatabase[item.itemID];

    auto font = AssetManager::getFont(AssetManager::FontID::PressStart2P);

    std::string name_str = item_data.name;
    std::string id_str = "ID: " + std::to_string(static_cast<uint32_t>(item.itemID));

    sf::Text nameText(AssetManager::getFont(AssetManager::FontID::PressStart2P), name_str, TEXT_SIZE);
    sf::Text idText(AssetManager::getFont(AssetManager::FontID::PressStart2P), id_str, TEXT_SIZE);

    nameText.setFillColor(getRarityColor(item_data.rarity));
    idText.setFillColor(sf::Color::White);

    float padding = static_cast<float>(TEXT_SIZE) * 0.4f;
    float lineSpacing = static_cast<float>(TEXT_SIZE) * 0.3f;

    sf::FloatRect nameBounds = nameText.getLocalBounds();
    sf::FloatRect idBounds = idText.getLocalBounds();

    float frameWidth = std::max(nameBounds.size.x, idBounds.size.x) + padding * 2.0f;
    float frameHeight = nameBounds.size.y + idBounds.size.y + lineSpacing + padding * 2.0f;

    sf::Vector2f framePos = position + sf::Vector2f(15.0f, 15.0f);

    sf::Vector2u windowSize = target.getSize();

    if(framePos.x + frameWidth > windowSize.x)
    {
        framePos.x = windowSize.x - frameWidth;
    }
    if(framePos.y + frameHeight > windowSize.y)
    {
        framePos.y = windowSize.y - frameHeight;
    }

    sf::RectangleShape frame({frameWidth, frameHeight});
    frame.setPosition(framePos);
    frame.setFillColor(sf::Color(0, 0, 0, 200));
    frame.setOutlineColor(sf::Color::White);
    frame.setOutlineThickness(1.0f);
    target.draw(frame);

    nameText.setPosition(framePos + sf::Vector2f(padding, padding));
    idText.setPosition(framePos + sf::Vector2f(padding, padding + nameBounds.size.y + lineSpacing));

    target.draw(nameText);
    target.draw(idText);

}

void renderBar(int max, int value, sf::Color primary_color, sf::Color secondary_color, sf::FloatRect bounds, sf::RenderTarget& target)
{
    sf::RectangleShape background(bounds.size);
    background.setPosition(bounds.position);
    background.setFillColor(sf::Color(72, 72, 72, 125));

    target.draw(background);

    const float factor = std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.0f, 1.0f);
    if (factor <= 0.0f) return;

    static const PerlinNoise3D bar_perlin(1337u);
    static sf::Clock bar_clock;
    const float t = bar_clock.getElapsedTime().asSeconds();

    const float filled_width = bounds.size.x * factor;

    const int cols = std::max(16, static_cast<int>(filled_width / 6.0f));
    const int rows = std::max(3, static_cast<int>(bounds.size.y / 4.0f));

    auto clip = [](float c) {
        return static_cast<std::uint8_t>(std::clamp(c, 0.0f, 255.0f));
    };

    auto sample = [&](float u, float v) {
        const sf::Color base = lerpColor(secondary_color, primary_color, u);

        const double n1 = bar_perlin.noise(u * 4.0, v * 2.5, t * 0.6);
        const double n2 = bar_perlin.noise(u * 9.0 + 11.3, v * 5.0, t * 1.4 + 7.7);
        const float n = static_cast<float>(n1 * 0.65 + n2 * 0.35);

        const float brightness = 0.65f + n * 0.75f;

        return sf::Color(
            clip(base.r * brightness),
            clip(base.g * brightness),
            clip(base.b * brightness),
            base.a
        );
    };

    for (int row = 0; row < rows; ++row)
    {
        const float v0 = static_cast<float>(row)     / static_cast<float>(rows);
        const float v1 = static_cast<float>(row + 1) / static_cast<float>(rows);

        sf::VertexArray strip(sf::PrimitiveType::TriangleStrip, (cols + 1) * 2);

        for (int col = 0; col <= cols; ++col)
        {
            const float u = static_cast<float>(col) / static_cast<float>(cols);

            const sf::Vector2f p0 = bounds.position + sf::Vector2f(filled_width * u, bounds.size.y * v0);
            const sf::Vector2f p1 = bounds.position + sf::Vector2f(filled_width * u, bounds.size.y * v1);

            strip[col * 2 + 0].position = p0;
            strip[col * 2 + 0].color    = sample(u, v0);
            strip[col * 2 + 1].position = p1;
            strip[col * 2 + 1].color    = sample(u, v1);
        }

        target.draw(strip);
    }
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

    float itemSize = slotWidth - slotSpacing;

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

    sf::Vector2u texture_size = {std::min(target.getSize().x / 10, MAX_MINIMAP_TEXTURE_WIDTH), std::min(target.getSize().x / 10, MAX_MINIMAP_TEXTURE_WIDTH)};

    sf::RenderTexture render_texture(texture_size);
    render_texture.clear(sf::Color::Transparent);

    sf::View view({0.0f, 0.0f}, {static_cast<float>(texture_size.x), static_cast<float>(texture_size.y)});
    view.setSize({view.getSize().x, -(view.getSize().y)});
    render_texture.setView(view);

    auto [sky_top, sky_bottom] = getSkyGradient(world.getDayTime() / World::DAY_CYCLE_DURATION);
    renderSky(render_texture, sky_top, sky_bottom);

    float unit_size = static_cast<float>(texture_size.x) / 80.0f;
    
    for(int i = positionToChunk(camera) - 5; i <= positionToChunk(camera) + 5; i++)
    {
        Chunk& chunk = world.getChunk(i);
        ChunkMesh& mesh = world.getChunkMeshes()[i];

        if(!chunk.generated || !mesh.built) continue;

        float translate_x = static_cast<float>((static_cast<double>(i * CHUNK_WIDTH) - camera.x)) * unit_size;
        float translate_y = static_cast<float>(-camera.y) * unit_size;

        sf::RenderStates states;

        states.texture = &BlockAtlas::getTexture();
        states.transform.translate({translate_x, translate_y});
        states.transform.scale({unit_size, unit_size});
        applyBlockAnimationShader(states);

        render_texture.draw(mesh.vertices, states);
    }

    render_texture.display();


    sf::RectangleShape minimap({static_cast<float>(target.getSize().x) / 10.0f, static_cast<float>(target.getSize().x) / 10.0f});

    minimap.setTexture(&render_texture.getTexture());

    minimap.setPosition({50.0f, 50.0f});

    minimap.setOutlineThickness(5.0f);
    minimap.setOutlineColor(sf::Color(72, 72, 72));

    target.draw(minimap);
}