#include "../include/Render.hpp"
#include "../include/MainGameState.hpp"
#include "../include/BlockAtlas.hpp"

void RenderWorld(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    int centerChunk = window.getView().getCenter().x / unit_size / CHUNK_WIDTH;

    sf::RenderStates states;
    states.texture = &BlockAtlas::getTexture();

    for (int i = centerChunk - 2; i <= centerChunk + 2; ++i)
    {
        Chunk& chunk = world.getChunk(i);

        if (!chunk.generated) continue;

        ChunkMesh& mesh = world.chunkMeshes[i];

        if (chunk.dirty || !mesh.built)
        {
            world.rebuildChunkMesh(i, unit_size);
        }

        window.draw(mesh.vertices, states);
    }
}

void RenderBlockOverlay(World& world, sf::RenderWindow& window)
{
    unsigned int unit_size = window.getSize().y / MainGameState::UNIT_SIZE_FACTOR;

    sf::Vector2i blockPos = getMouseBlockPosition(world, window);
    int blockX = blockPos.x;
    int blockY = blockPos.y;

    if(world.getBlock(blockX, blockY).id != BlockID::Air && isBlockInRange(entityWithID(world.getPlayerID(), world).getComponent<TransformComponent>(), blockPos, 4.0f))
    {
        sf::Sprite sprite(AssetManager::getTexture(8));
        sprite.setPosition({
            blockX * static_cast<float>(unit_size),
            blockY * static_cast<float>(unit_size)
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

    unsigned int unit_size = windowSize.y / MainGameState::UNIT_SIZE_FACTOR;

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