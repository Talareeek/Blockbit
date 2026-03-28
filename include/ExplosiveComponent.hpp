#ifndef EXPLOSIVE_COMPONENT_HPP
#define EXPLOSIVE_COMPONENT_HPP

struct ExplosiveComponent
{
    float force;

    bool destroyBlocks = true;
    bool damageEntities = true;

    float fuseTime = 3.0f;
    float timer = 0.0f;

};

#endif // EXPLOSIVE_COMPONENT_HPP