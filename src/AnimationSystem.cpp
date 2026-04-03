#include "../include/AnimationSystem.hpp"
#include "../include/AnimationComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/World.hpp"

void AnimationSystem(World& world, float dt)
{
    for(auto& entity : world.getEntities())
    {
        if(!entity.hasComponent<AnimationComponent>() || !entity.hasComponent<RenderComponent>()) continue;

        auto& animation = entity.getComponent<AnimationComponent>();
        auto& render = entity.getComponent<RenderComponent>();
        auto& clip = animation.animations[animation.currentState];

        animation.timer += dt;

        if(animation.timer >= clip.frameTime)
        {
            animation.timer -= clip.frameTime;
            animation.currentFrame++;

            if(animation.currentFrame >= clip.frameCount)
            {
                if(clip.loop)
                    animation.currentFrame = 0;
                else
                    animation.currentFrame = clip.frameCount - 1;
            }

            uint32_t frame = clip.startFrame + animation.currentFrame;

            render.uv =
            {
                {int(frame * animation.frameSize.x), 0},
                animation.frameSize
            };
        }
    }
}