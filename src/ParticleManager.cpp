#include "../include/ParticleManager.hpp"

#include "../include/ClientWorld.hpp"
#include "../include/AssetManager.hpp"
#include "../include/GameCommon.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
    constexpr float GRAVITY = 18.0f;        // blocks/s^2
    constexpr float DRAG    = 0.92f;        // per-second multiplier

    struct Profile
    {
        float    lifetime_min;
        float    lifetime_max;
        float    scale_min;
        float    scale_max;
        sf::Color color_min;
        sf::Color color_max;
        uint8_t   flags;
    };

    Profile profileFor(ParticleEffect effect)
    {
        using F = Particle;
        switch (effect)
        {
            case ParticleEffect::BlockDust:
                // Color is replaced by the block texture tile; alpha stays full
                // and fade handles the dimming. Keep Gravity+Collides so they
                // look like real debris.
                return {0.45f, 0.75f, 0.14f, 0.22f,
                        sf::Color(255, 255, 255, 255),
                        sf::Color(255, 255, 255, 255),
                        static_cast<uint8_t>(F::Gravity | F::Collides | F::Fades)};
            case ParticleEffect::Explosion:
                return {0.5f, 0.9f, 0.25f, 0.55f,
                        sf::Color(255, 200, 80, 220),
                        sf::Color(180,  80, 30, 180),
                        static_cast<uint8_t>(F::Fades | F::Rises)};
            case ParticleEffect::Smoke:
                return {0.7f, 1.4f, 0.18f, 0.35f,
                        sf::Color(90,  90,  90, 200),
                        sf::Color(50,  50,  50, 160),
                        static_cast<uint8_t>(F::Fades | F::Rises)};
            case ParticleEffect::Flame:
                return {0.3f, 0.6f, 0.08f, 0.18f,
                        sf::Color(255, 220, 90, 230),
                        sf::Color(255, 120, 40, 200),
                        static_cast<uint8_t>(F::Fades | F::Rises | F::Flicker)};
            case ParticleEffect::Spark:
                return {0.15f, 0.35f, 0.04f, 0.09f,
                        sf::Color(255, 240, 150, 255),
                        sf::Color(255, 180,  60, 230),
                        static_cast<uint8_t>(F::Gravity | F::Fades)};
            case ParticleEffect::Splash:
                return {0.3f, 0.6f, 0.06f, 0.12f,
                        sf::Color(140, 200, 255, 220),
                        sf::Color( 60, 140, 220, 200),
                        static_cast<uint8_t>(F::Gravity | F::Fades)};
            case ParticleEffect::Ember:
                return {0.4f, 0.9f, 0.05f, 0.10f,
                        sf::Color(255, 180,  80, 230),
                        sf::Color(200,  60,  20, 200),
                        static_cast<uint8_t>(F::Rises | F::Fades | F::Flicker)};
            case ParticleEffect::Damage:
                return {0.3f, 0.5f, 0.08f, 0.14f,
                        sf::Color(200,  30,  30, 220),
                        sf::Color(140,  10,  10, 200),
                        static_cast<uint8_t>(F::Gravity | F::Fades)};
            case ParticleEffect::Bubble:
                return {0.6f, 1.2f, 0.05f, 0.09f,
                        sf::Color(200, 230, 255, 180),
                        sf::Color(150, 200, 240, 140),
                        static_cast<uint8_t>(F::Rises | F::Fades)};
        }
        return {0.5f, 1.0f, 0.1f, 0.2f,
                sf::Color::White, sf::Color::White,
                static_cast<uint8_t>(F::Fades)};
    }

    sf::Color lerpColor(sf::Color a, sf::Color b, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        auto mix = [t](uint8_t x, uint8_t y)
        {
            return static_cast<uint8_t>(std::round(x + (y - x) * t));
        };
        return sf::Color(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a));
    }
}

void ParticleManager::spawn(const ParticleSpawnParams& params)
{
    uint16_t count = params.count == 0 ? 1 : params.count;
    for (uint16_t i = 0; i < count; ++i)
    {
        if (particles.size() >= MAX_ALIVE) break;
        emitOne(params);
    }
}

void ParticleManager::emitOne(const ParticleSpawnParams& params)
{
    Profile profile = profileFor(params.effect);

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> signed_unit(-1.0f, 1.0f);

    Particle p;

    float angle = unit(rng) * 6.2831853f;
    float speed = unit(rng) * params.spread;
    p.velocity.x = params.base_velocity.x + std::cos(angle) * speed;
    p.velocity.y = params.base_velocity.y + std::sin(angle) * speed;

    p.position.x = params.position.x + signed_unit(rng) * 0.1f;
    p.position.y = params.position.y + signed_unit(rng) * 0.1f;

    p.max_lifetime = profile.lifetime_min + unit(rng) * (profile.lifetime_max - profile.lifetime_min);
    p.lifetime     = p.max_lifetime;

    p.scale        = profile.scale_min + unit(rng) * (profile.scale_max - profile.scale_min);
    p.color        = lerpColor(profile.color_min, profile.color_max, unit(rng));
    p.flags        = profile.flags;

    if (params.effect == ParticleEffect::BlockDust && params.block_id != BlockID::Air)
    {
        auto it = blockDatabase.find(params.block_id);
        if (it != blockDatabase.end() && it->second.render.has_value())
        {
            try
            {
                const sf::Texture& tex = AssetManager::getGameTexture(it->second.render->texture);
                p.texture = &tex;

                // Pick a random 4x4 tile out of the first 16x16 frame; good
                // enough for every block whose first frame is the "neutral"
                // variant. For animated/multi-frame textures this still looks
                // right because frames are stacked horizontally at 16px.
                auto size = tex.getSize();
                int tile = 4;
                int cols = std::max(1, static_cast<int>(size.x / tile));
                int rows = std::max(1, static_cast<int>(16 / tile));
                std::uniform_int_distribution<int> col_dist(0, std::min(cols, 16 / tile) - 1);
                std::uniform_int_distribution<int> row_dist(0, rows - 1);
                int cx = col_dist(rng) * tile;
                int cy = row_dist(rng) * tile;
                p.texture_rect = sf::IntRect({cx, cy}, {tile, tile});
            }
            catch (...)
            {
                // Texture not loaded — fall back to plain color.
                p.texture = nullptr;
            }
        }
    }

    particles.push_back(p);
}

void ParticleManager::tick(float dt, ClientWorld& world)
{
    if (dt <= 0.0f) return;

    float drag_factor = std::pow(DRAG, dt);

    std::uniform_real_distribution<float> signed_unit(-1.0f, 1.0f);

    for (std::size_t i = 0; i < particles.size();)
    {
        Particle& p = particles[i];

        p.lifetime -= dt;
        if (p.lifetime <= 0.0f)
        {
            particles[i] = particles.back();
            particles.pop_back();
            continue;
        }

        if (p.flags & Particle::Gravity) p.velocity.y -= GRAVITY * dt;
        if (p.flags & Particle::Rises)   p.velocity.y += 2.0f * dt;
        if (p.flags & Particle::Flicker) p.velocity.x += signed_unit(rng) * 2.0f * dt;

        p.velocity.x *= drag_factor;
        p.velocity.y *= drag_factor;

        sf::Vector2f next = p.position + p.velocity * dt;

        if (p.flags & Particle::Collides)
        {
            int bx = static_cast<int>(std::floor(next.x));
            int by = static_cast<int>(std::floor(next.y));
            Block block = world.getBlock(bx, by);
            auto it = blockDatabase.find(block.id);
            if (it != blockDatabase.end() && it->second.solid)
            {
                // Simple axis-aligned reflect-and-damp — flipping the component
                // that would step into the solid cell and shaving energy so the
                // particle settles instead of bouncing forever.
                int cx = static_cast<int>(std::floor(p.position.x));
                int cy = static_cast<int>(std::floor(p.position.y));
                if (cx != bx) p.velocity.x = -p.velocity.x * 0.2f;
                if (cy != by) p.velocity.y = -p.velocity.y * 0.2f;
                p.lifetime = std::min(p.lifetime, 0.15f);
                next = p.position + p.velocity * dt;
            }
        }

        p.position = next;
        ++i;
    }
}

void ParticleManager::render(sf::RenderWindow& window, sf::Vector2<double> camera)
{
    if (particles.empty()) return;

    unsigned int unit_size = window.getSize().y / WORLD_UNIT_SIZE_FACTOR;

    // Batch per-texture (nullptr groups together). Each particle is 2 tris = 6
    // verts. One draw call per bucket keeps this cheap even with the hard cap.
    std::unordered_map<const sf::Texture*, sf::VertexArray> buckets;

    for (const Particle& p : particles)
    {
        auto [it, inserted] = buckets.try_emplace(p.texture, sf::PrimitiveType::Triangles);
        sf::VertexArray& verts = it->second;

        float fade = 1.0f;
        if (p.flags & Particle::Fades) fade = std::clamp(p.lifetime / std::max(p.max_lifetime, 0.0001f), 0.0f, 1.0f);

        sf::Color color = p.color;
        color.a = static_cast<uint8_t>(std::round(color.a * fade));

        sf::Vector2f screen = {
            static_cast<float>(p.position.x - static_cast<float>(camera.x)),
            static_cast<float>(p.position.y - static_cast<float>(camera.y))
        };
        float half = p.scale * 0.5f;
        float x0 = (screen.x - half) * unit_size;
        float x1 = (screen.x + half) * unit_size;
        float y0 = (screen.y - half) * unit_size;
        float y1 = (screen.y + half) * unit_size;

        float u0 = 0, u1 = 0, v0 = 0, v1 = 0;
        if (p.texture)
        {
            u0 = static_cast<float>(p.texture_rect.position.x);
            u1 = static_cast<float>(p.texture_rect.position.x + p.texture_rect.size.x);
            v0 = static_cast<float>(p.texture_rect.position.y);
            v1 = static_cast<float>(p.texture_rect.position.y + p.texture_rect.size.y);
        }

        verts.append(sf::Vertex{{x0, y0}, color, {u0, v0}});
        verts.append(sf::Vertex{{x1, y0}, color, {u1, v0}});
        verts.append(sf::Vertex{{x1, y1}, color, {u1, v1}});

        verts.append(sf::Vertex{{x0, y0}, color, {u0, v0}});
        verts.append(sf::Vertex{{x1, y1}, color, {u1, v1}});
        verts.append(sf::Vertex{{x0, y1}, color, {u0, v1}});
    }

    for (auto& [tex, verts] : buckets)
    {
        sf::RenderStates states;
        if (tex) states.texture = tex;
        window.draw(verts, states);
    }
}
