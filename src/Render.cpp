#include "../include/Render.hpp"
#include "../include/GameCommon.hpp"
#include "../include/Entity.hpp"
#include "../include/World.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/NetworkInterpolationComponent.hpp"

#include <random>
#include <numbers>

std::unordered_map<int, ChunkMesh> chunkMeshes;

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

    void appendQuad(sf::VertexArray& va, const sf::Vector2f& pos, const sf::Vector2f& size, const sf::IntRect& uv)
    {
        sf::Vertex v0, v1, v2, v3;

        const sf::Vector2f uvPos  = sf::Vector2f(static_cast<float>(uv.position.x), static_cast<float>(uv.position.y));
        const sf::Vector2f uvSize = sf::Vector2f(static_cast<float>(uv.size.x), static_cast<float>(uv.size.y));

        const float u0 = uvPos.x;
        const float v0y = uvPos.y;
        const float u1 = uvPos.x + uvSize.x;
        const float v1y = uvPos.y + uvSize.y;

        v0.position = {pos.x, pos.y};
        v1.position = {pos.x + size.x, pos.y};
        v2.position = {pos.x + size.x, pos.y + size.y};
        v3.position = {pos.x, pos.y + size.y};

        v0.texCoords = {u0, v1y};
        v1.texCoords = {u1, v1y};
        v2.texCoords = {u1, v0y};
        v3.texCoords = {u0, v0y};

        va.append(v0);
        va.append(v1);
        va.append(v2);

        va.append(v0);
        va.append(v2);
        va.append(v3);
    }
}

void rebuildChunkMesh(World& world, int chunk_position, unsigned int unit_size)
{
    Chunk& chunk = world.getChunk(chunk_position);
    ChunkMesh& mesh = chunkMeshes[chunk_position];

    mesh.vertices.clear();

    const float size = (float)unit_size;

    for (int y = 0; y < CHUNK_HEIGHT; y++)
    {
        for (int x = 0; x < CHUNK_WIDTH; x++)
        {
            Block& block = chunk.blocks[y][x];

            if (block.id == BlockID::Air)
                continue;

            auto texID = blockDatabase[block.id].texture;

            const sf::IntRect& uv = BlockAtlas::getUV(texID);

            float worldX = x * size;
            float worldY = y * size;

            float heightFactor = 1.0f;

            if (block.id == BlockID::Water)
            {
                uint8_t level = block.metadata;

                if (level == 9)
                    level = 8;

                BlockID above =
                    world.getBlock(
                        chunk_position * CHUNK_WIDTH + x,
                        y + 1).id;

                if (above != BlockID::Water)
                    heightFactor = level / 9.0f;
            }

            sf::Vector2f pos(worldX, worldY);
            sf::Vector2f blockSize(size, size * heightFactor);

            appendQuad(mesh.vertices, pos, blockSize, uv);
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

void renderSky(sf::RenderWindow& window, sf::Color top, sf::Color bottom)
{
    const float width = static_cast<float>(window.getSize().x);
    const float height = static_cast<float>(window.getSize().y);

    sf::VertexArray sky(sf::PrimitiveType::TriangleStrip, 4);

    sky[0].position = {0.0f, 0.0f};
    sky[1].position = {width,    0.0f};
    sky[2].position = {0.0f, height};
    sky[3].position = {width,    height};

    sky[0].color = top;
    sky[1].color = top;
    sky[2].color = bottom;
    sky[3].color = bottom;

    sf::View previous = window.getView();
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {width, height})));
    window.draw(sky);
    window.setView(previous);
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

void RenderWorld(World& world, const sf::Vector2<double> camera, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / WORLD_UNIT_SIZE_FACTOR;

    int centerChunk = static_cast<int>(std::floor(camera.x / static_cast<double>(CHUNK_WIDTH)));

    for (int i = centerChunk - 2; i <= centerChunk + 2; ++i)
    {
        Chunk& chunk = world.getChunk(i);

        if (!chunk.generated) continue;

        ChunkMesh& mesh = chunkMeshes[i];

        if (chunk.meshDirty || !mesh.built)
        {
            rebuildChunkMesh(world, i, unit_size);
        }

        float tx = static_cast<float>((static_cast<double>(i * CHUNK_WIDTH) - camera.x) * static_cast<double>(unit_size));
        float ty = static_cast<float>(-camera.y * static_cast<double>(unit_size));

        sf::RenderStates states;
        states.texture = &BlockAtlas::getTexture();
        states.transform.translate({tx, ty});

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

void RenderBlockOutline(const sf::Vector2<double> camera, const sf::Vector2i block, sf::RenderWindow& window)
{
    float unit_size = static_cast<float>(window.getSize().y) / static_cast<float>(WORLD_UNIT_SIZE_FACTOR);

    sf::RectangleShape shape({unit_size * (14.0f / 16.0f), unit_size * (14.0f / 16.0f)});
    
    shape.setFillColor(sf::Color::Transparent);

    shape.setOutlineColor(sf::Color::Black);
    shape.setOutlineThickness(unit_size * (1.0f / 16.0f));

    shape.setPosition(
        {
            (static_cast<float>(static_cast<double>(block.x) - camera.x) + (1.0f / 16.0f)) * unit_size,
            (static_cast<float>(static_cast<double>(block.y) - camera.y) + (1.0f / 16.0f)) * unit_size
        }
    );

    window.draw(shape);
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

    std::uniform_real_distribution<float> baseDist(0.0f,    200.0f);
    std::uniform_real_distribution<float> scaleDist(0.0f,    100.0f);
    std::uniform_real_distribution<float> freqDist (0.001f,  0.200f);
    std::uniform_real_distribution<float> ampDist  (0.0f,    2.0f);
    std::uniform_real_distribution<float> persDist (0.0f,    1.0f);

    GenerationProperties props{};
    props.flat = false;
    props.base_height = baseDist(rng);
    props.height_scale = scaleDist(rng);
    props.frequency = freqDist(rng);
    props.amplitude = ampDist(rng);
    props.persistence = persDist(rng);

    PerlinNoise perlin(seed);

    auto heightAt = [&](float x) -> float
    {
        float total = 0.0f;
        float f = props.frequency;
        float a = props.amplitude;
        float p = props.persistence;

        for (int i = 0; i < 4; ++i)
        {
            total += perlin.noise(x * f, 0.0f) * a;
            a *= p;
            f *= 2.0f;
        }
        return props.base_height + total * props.height_scale;
    };

    constexpr unsigned int OUT_W = 1920;
    constexpr unsigned int OUT_H = 1080;
    constexpr unsigned int UNIT_SIZE = 45;

    const float viewBlocksW = static_cast<float>(OUT_W) / UNIT_SIZE;
    const float viewBlocksH = static_cast<float>(OUT_H) / UNIT_SIZE;

    const float cameraX = 0.0f;

    const int firstCol = static_cast<int>(std::floor(cameraX - viewBlocksW * 0.5f)) - 2;
    const int lastCol  = static_cast<int>(std::ceil (cameraX + viewBlocksW * 0.5f)) + 2;

    float minH =  std::numeric_limits<float>::infinity();
    for (int x = firstCol; x <= lastCol; ++x)
    {
        minH = std::min(minH, heightAt(static_cast<float>(x)));
    }

    const float bottomView = std::floor(minH) - 3.0f;
    const float cameraY = bottomView + viewBlocksH * 0.5f;

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
        const int h = static_cast<int>(std::floor(heightAt(static_cast<float>(wx))));

        const float px = (static_cast<float>(wx) - cameraX + viewBlocksW * 0.5f) * UNIT_SIZE;

        for (int wy = minY; wy <= maxY; ++wy)
        {
            if (wy >= h) continue;

            const BlockID id = (wy == h - 1) ? BlockID::Grass : BlockID::Dirt;

            const auto texID = blockDatabase[id].texture;
            sf::Sprite sprite(AssetManager::getGameTexture(texID));

            const float py = (cameraY + viewBlocksH * 0.5f - static_cast<float>(wy + 1)) * UNIT_SIZE;
            sprite.setPosition({px, py});

            const auto texSize = sprite.getTexture().getSize();
            sprite.setScale({
                static_cast<float>(UNIT_SIZE) / texSize.x,
                static_cast<float>(UNIT_SIZE) / texSize.y
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

    const float W = static_cast<float>(window.getSize().x);
    const float H = static_cast<float>(window.getSize().y);

    if (stars.empty())
    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> distX(0.0f, W);
        std::uniform_real_distribution<float> distY(00.f, H/* * 0.75f*/);
        std::uniform_int_distribution<int> distSize(2, 4);
        std::uniform_real_distribution<float> distBright(0.4f, 1.0f);
        std::uniform_real_distribution<float> distSpeed(0.5f, 2.0f);
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

    sf::VertexArray quads(sf::PrimitiveType::Triangles, stars.size() * 6);
    std::size_t vi = 0;

    for (const Star& s : stars)
    {
        float twinkle = 0.75f + 0.25f * std::sin(daytime * s.twinkleSpeed + s.twinklePhase);
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