#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include "BBT.hpp"

#include <cstdint>

enum class ComponentSide : uint8_t
{
    Server,
    Client,
    Both
};

struct Component
{
    bool dirty = true;

    virtual std::string name() const = 0;

    virtual ComponentSide side() const { return ComponentSide::Both; }

    virtual Tag serialize() const = 0;
    virtual void deserialize(const Tag& tag) = 0;

    virtual std::unique_ptr<Component> clone() const = 0;

    virtual ~Component() = default;
};

#endif // COMPONENT_HPP
