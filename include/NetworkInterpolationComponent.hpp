#ifndef NETWORK_INTERPOLATION_COMPONENT_HPP
#define NETWORK_INTERPOLATION_COMPONENT_HPP

#include "Component.hpp"

#include <deque>

struct NetworkInterpolationComponent : public Component
{
    struct Sample
    {
        uint64_t tick;
        sf::Vector2<double> position;
        sf::Angle rotation;
    };

    std::deque<Sample> history;
    static constexpr std::size_t MAX_SAMPLES = 20;

    void pushSample(uint64_t tick, sf::Vector2<double> position, sf::Angle rotation)
    {
        history.push_back({tick, position, rotation});
        while (history.size() > MAX_SAMPLES) history.pop_front();
    }

    std::string name() const override { return "NetworkInterpolationComponent"; }
    Tag serialize() const override { return Tag(TagCompound{}); }
    void deserialize(const Tag&) override {}

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<NetworkInterpolationComponent>(*this);
    }
};

#endif // NETWORK_INTERPOLATION_COMPONENT