#ifndef PHYSICS_SYSTEM_HPP
#define PHYSICS_SYSTEM_HPP

#include <vector>
#include "Entity.hpp"
#include "World.hpp"

extern void PhysicsSystem(std::vector<Entity>& entities, World& world, float deltaTime);

#endif // PHYSICS_SYSTEM_HPP