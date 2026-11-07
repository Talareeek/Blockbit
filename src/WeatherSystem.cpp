#include "../include/WeatherSystem.hpp"
#include "../include/ServerWorld.hpp"
#include "../include/TransformComponent.hpp"
#include "../include/LightningBoltComponent.hpp"
#include "../include/UUID.hpp"

#include <random>

namespace
{
    // Weather state is a small cycle: Clear -> Rain -> (Rain|Thunder) -> Rain -> Clear.
    // Thunder is reachable only from Rain, mirroring Minecraft's "thunder implies
    // rain" invariant without carrying two independent timers on the wire.
    constexpr float CLEAR_MIN_SECONDS   = 300.0f;   // 5 min
    constexpr float CLEAR_MAX_SECONDS   = 900.0f;   // 15 min
    constexpr float RAIN_MIN_SECONDS    = 180.0f;   // 3 min
    constexpr float RAIN_MAX_SECONDS    = 480.0f;   // 8 min
    constexpr float THUNDER_MIN_SECONDS = 60.0f;    // 1 min
    constexpr float THUNDER_MAX_SECONDS = 180.0f;   // 3 min

    // When a rain phase ends, probability that the next state is Thunder rather
    // than Clear. Thunder phases always return to Rain (never straight to Clear).
    constexpr float THUNDER_ESCALATION_CHANCE = 0.25f;

    // Lightning spawn tuning. One roll per loaded chunk per second — scale that
    // with dt so the frequency stays stable regardless of server tick rate.
    // At 0.03/s/chunk and ~17 loaded chunks you get a strike every ~2s during
    // a storm, which reads as "frequent but not constant".
    constexpr float LIGHTNING_STRIKES_PER_CHUNK_PER_SECOND = 0.03f;

    // Short on purpose — the entity exists just long enough to be guaranteed
    // delivered to clients before deletion. Visually the client fades it out
    // over this duration.
    constexpr float LIGHTNING_LIFETIME_SECONDS = 0.5f;

    std::mt19937& rng()
    {
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }

    float randomInRange(float min_value, float max_value)
    {
        std::uniform_real_distribution<float> dist(min_value, max_value);
        return dist(rng());
    }

    bool roll(float probability)
    {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng()) < probability;
    }

    float rollDurationFor(WeatherState state)
    {
        switch (state)
        {
            case WeatherState::Clear:   return randomInRange(CLEAR_MIN_SECONDS,   CLEAR_MAX_SECONDS);
            case WeatherState::Rain:    return randomInRange(RAIN_MIN_SECONDS,    RAIN_MAX_SECONDS);
            case WeatherState::Thunder: return randomInRange(THUNDER_MIN_SECONDS, THUNDER_MAX_SECONDS);
        }
        return CLEAR_MIN_SECONDS;
    }

    WeatherState nextStateAfter(WeatherState current)
    {
        switch (current)
        {
            case WeatherState::Clear:
                return WeatherState::Rain;
            case WeatherState::Rain:
                return roll(THUNDER_ESCALATION_CHANCE) ? WeatherState::Thunder : WeatherState::Clear;
            case WeatherState::Thunder:
                return WeatherState::Rain;
        }
        return WeatherState::Clear;
    }

    // Top non-Air block in this column. Lightning anchors just above it so the
    // zigzag reads as grounded instead of hanging in open air.
    int topOpaqueY(ServerWorld& world, int wx)
    {
        for (int y = CHUNK_HEIGHT - 1; y >= 0; --y)
        {
            if (world.getBlock(wx, y).id != BlockID::Air) return y;
        }
        return -1;
    }

    void trySpawnLightning(ServerWorld& world, float dt)
    {
        // One Bernoulli trial per loaded chunk per tick, probability scaled by
        // dt. Thunder is a precondition — lightning can't strike in clear or
        // merely rainy weather.
        const float p_per_chunk = LIGHTNING_STRIKES_PER_CHUNK_PER_SECOND * dt;

        auto& chunks = world.getChunks();
        for (int chunk_position : world.loadedChunks())
        {
            if (!roll(p_per_chunk)) continue;

            auto chunk_it = chunks.find(chunk_position);
            if (chunk_it == chunks.end() || !chunk_it->second.generated) continue;

            std::uniform_int_distribution<int> col_dist(0, CHUNK_WIDTH - 1);
            const int local_x = col_dist(rng());
            const int wx      = chunk_position * CHUNK_WIDTH + local_x;

            // Biome gating: no strikes where it isn't raining locally. Desert
            // and snow biomes stay lightning-free, matching the precipitation
            // mask the client uses for rendering.
            const Biome biome = chunk_it->second.biomes[local_x];
            const Precipitation precip = precipitationFor(WeatherState::Thunder, biome);
            if (precip != Precipitation::Rain) continue;

            const int top_y = topOpaqueY(world, wx);
            if (top_y < 0) continue;

            Entity bolt(generateUUID());

            // Hitbox is a thin vertical sliver anchored at the strike point —
            // the physics engine ignores it anyway (no PhysicsComponent), but
            // this lets debug renderers draw something sensible.
            TransformComponent transform(
                sf::Vector2<double>{static_cast<double>(wx), static_cast<double>(top_y + 1)},
                sf::Vector2<double>{0.2, 1.0});
            bolt.addComponent<TransformComponent>(std::move(transform));

            LightningBoltComponent lightning;
            lightning.lifetime_remaining = LIGHTNING_LIFETIME_SECONDS;
            lightning.seed               = static_cast<uint32_t>(rng()());
            lightning.height             = randomInRange(32.0f, 48.0f);
            bolt.addComponent<LightningBoltComponent>(std::move(lightning));

            world.addEntity(std::move(bolt));
        }
    }

    void tickLightningEntities(ServerWorld& world, float dt)
    {
        std::vector<UUID> to_remove;

        for (auto& [id, entity] : world.getEntities())
        {
            if (!entity.hasComponent<LightningBoltComponent>()) continue;

            auto& bolt = entity.getComponent<LightningBoltComponent>();
            bolt.lifetime_remaining -= dt;
            bolt.dirty = true;

            if (bolt.lifetime_remaining <= 0.0f) to_remove.push_back(id);
        }

        for (UUID id : to_remove) world.removeEntity(id);
    }
}

void WeatherSystem(ServerWorld& world, float dt)
{
    // Lazy-seed the timer on first tick of a fresh world; load() may have left
    // it at the persisted value, in which case we keep that.
    if (world.weatherTimer <= 0.0f)
    {
        world.weather = nextStateAfter(world.weather);
        world.weatherTimer = rollDurationFor(world.weather);
    }
    else
    {
        world.weatherTimer -= dt;
    }

    if (world.weather == WeatherState::Thunder) trySpawnLightning(world, dt);
    tickLightningEntities(world, dt);
}
