#ifndef COMPONENT_HPP
#define COMPONENT_HPP

#include "BBT.hpp"

struct Component
{
    virtual std::string name() const = 0;

    virtual Tag serialize() const;
    virtual void deserialize(const Tag& tag);
};

#endif // COMPONENT_HPP