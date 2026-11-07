#ifndef PARTICLE_MANAGER_HPP
#define PARTICLE_MANAGER_HPP

#include "Particle.hpp"
#include "Block.hpp"

#include <SFML/Graphics.hpp>

#include <vector>
#include <random>
#include <cstdint>

class ClientWorld;

// Keep this in sync with ParticleBurstPacket on the server. Values are wire
// IDs — do not renumber.
enum class ParticleEffect : uint16_t
{
    BlockDust  = 0,
    Explosion  = 1,
    Smoke      = 2,
    Flame      = 3,
    Spark      = 4,
    Splash     = 5,
    Ember      = 6,
    Damage     = 7,
    Bubble     = 8,
};

struct ParticleSpawnParams
{
    ParticleEffect effect = ParticleEffect::Smoke;
    sf::Vector2f   position{};

    uint16_t       count   = 1;
    float          spread  = 1.0f;  // velocity jitter magnitude (blocks/s)
    sf::Vector2f   base_velocity{}; // bias applied on top of jitter

    // For BlockDust: source block supplies the sprite. Ignored otherwise.
    BlockID        block_id = BlockID::Air;
};

class ParticleManager
{
public:

    void spawn(const ParticleSpawnParams& params);
    void tick(float dt, ClientWorld& world);
    void render(sf::RenderWindow& window, sf::Vector2<double> camera);

    void clear() { particles.clear(); }
    std::size_t size() const { return particles.size(); }

    // Keep a hard cap so a pathological explosion in the middle of the view
    // doesn't balloon memory or stall rendering. Picked to comfortably cover
    // several concurrent explosions plus ambient.
    static constexpr std::size_t MAX_ALIVE = 2000;

private:

    std::vector<Particle> particles;
    std::mt19937 rng{std::random_device{}()};

    void emitOne(const ParticleSpawnParams& params);
};

#endif // PARTICLE_MANAGER_HPP
