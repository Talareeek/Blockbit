#ifndef PHYSICS_SYSTEM_HPP
#define PHYSICS_SYSTEM_HPP

#include "Entity.hpp"
#include "World.hpp"
#include "TransformComponent.hpp"
#include "UUID.hpp"

extern bool isSubmerged(World& world, const TransformComponent& transform);

// `skip_entity` is a workaround for client-side prediction: the locally
// controlled player entity is integrated through sendTickInputs +
// reconciliation (stepEntityPhysics), not through this system, so passing
// its UUID keeps PhysicsSystem from stepping it a second time per tick.
// Server passes UUID{} which never matches a real entity.
extern void PhysicsSystem(World& world, float deltaTime, const UUID& skip_entity = UUID{});

// Per-entity physics step. Advances velocity + position by deltaTime using the
// same math as PhysicsSystem's inner loop. Used by the client reconciliation
// path to replay pending unacknowledged inputs on top of a fresh snapshot
// without double-stepping every other physics entity in the world.
extern void stepEntityPhysics(Entity& entity, World& world, float deltaTime);

#endif // PHYSICS_SYSTEM_HPP