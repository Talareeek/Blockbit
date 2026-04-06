#ifndef ANIMATION_COMPONENT_HPP
#define ANIMATION_COMPONENT_HPP

#include <cstdint>
#include <unordered_map>
#include <string>
#include <sstream>

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

    std::string serialize()
    {
        std::string output;

        for(const auto& [state, clip] : animations)
        {
            output += std::to_string(static_cast<uint32_t>(state)) + ' ';
            output += std::to_string(clip.startFrame) + ' ';
            output += std::to_string(clip.frameCount) + ' ';
            output += std::to_string(clip.frameTime) + ' ';
            output += std::to_string(clip.loop ? 1 : 0) + '\n';
        }

        output += std::to_string(static_cast<uint32_t>(currentState)) + '\n';
        output += std::to_string(direction == Direction::Left ? 0 : 1) + '\n';
        output += std::to_string(timer) + '\n';
        output += std::to_string(currentFrame) + '\n';
        output += std::to_string(frameSize.x) + ' ' + std::to_string(frameSize.y) + '\n';

        return output;
    }

    void deserialize(const std::string& data)
    {
        std::istringstream iss(data);
        animations.clear();

        while(iss.peek() != EOF)
        {
            uint32_t stateInt, startFrame, frameCount;
            float frameTime;
            int loopInt;

            iss >> stateInt >> startFrame >> frameCount >> frameTime >> loopInt;

            if(iss.fail()) break;

            AnimationState state = static_cast<AnimationState>(stateInt);
            animations[state] = {startFrame, frameCount, frameTime, loopInt != 0};
        }

        uint32_t currentStateInt;
        int directionInt;
        iss >> currentStateInt >> directionInt >> timer >> currentFrame >> frameSize.x >> frameSize.y;

        currentState = static_cast<AnimationState>(currentStateInt);
        direction = directionInt == 0 ? Direction::Left : Direction::Right;
    }
};


#endif // ANIMATION_COMPONENT_HPP