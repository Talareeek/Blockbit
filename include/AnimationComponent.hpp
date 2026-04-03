#ifndef ANIMATION_COMPONENT_HPP
#define ANIMATION_COMPONENT_HPP

#include <cstdint>
#include <unordered_map>

#include <SFML/System/Vector2.hpp>

struct AnimationClip
{
    uint32_t startFrame;
    uint32_t frameCount;

    float frameTime;
    bool loop;
};

enum class AnimationState
{
    Idle,
    Looking,
    Walking,
    Running,
    Jumping,
    Using,
    Flying
};

struct AnimationComponent
{
    std::unordered_map<AnimationState, AnimationClip> animations;

    AnimationState currentState;
    
    enum class Direction
    {
        Left,
        Right
    } direction;

    float timer = 0;
    uint32_t currentFrame = 0;

    sf::Vector2i frameSize; 
};


#endif // ANIMATION_COMPONENT_HPP