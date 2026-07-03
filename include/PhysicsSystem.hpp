#ifndef PHYSICS_SYSTEM_HPP
#define PHYSICS_SYSTEM_HPP

#include <vector>
#include "Entity.hpp"
#include "World.hpp"
#include "TransformComponent.hpp"

extern bool isSubmerged(World& world, const TransformComponent& transform);

extern void PhysicsSystem(std::vector<Entity>& entities, World& world, float deltaTime);

#endif // PHYSICS_SYSTEM_HPP