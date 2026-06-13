#include "../include/DaycycleSystem.hpp"

void DaycycleSystem(World& world, float dt)
{
    world.dayTime += dt;
    if(world.dayTime >= World::DAY_CYCLE_DURATION)
    {
        world.dayTime -= World::DAY_CYCLE_DURATION;
        world.days++;
    }
}