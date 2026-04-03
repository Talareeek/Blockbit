#include "../include/AnimationComponent.hpp"

void setAnimation(AnimationComponent& anim, const AnimationState& state)
{
    if(anim.currentState == state)
        return;

    anim.currentState = state;
    anim.currentFrame = 0;
    anim.timer = 0;
}