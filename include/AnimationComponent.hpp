#ifndef ANIMATION_COMPONENT_HPP
#define ANIMATION_COMPONENT_HPP

#include "Component.hpp"

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
    Walking,
    Running,
    Jumping,
    Using,
    Flying
};

struct AnimationComponent : public Component
{
    std::unordered_map<AnimationState, AnimationClip> animations;

    AnimationState currentState = AnimationState::Idle;

    enum class Direction
    {
        Left,
        Right
    } direction = Direction::Right;

    float timer = 0;
    uint32_t currentFrame = 0;

    sf::Vector2i frameSize; 

    std::string serialize()
    {
        std::string output;

        output += std::to_string(animations.size()) + '\n';

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

        size_t count = 0;
        iss >> count;

        for(size_t i = 0; i < count; i++)
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

    std::string name() const override
    {
        return "AnimationComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        TagList animations_list;

        for(const auto& [state, clip] : animations)
        {
            TagCompound animation;

            animation["state"] = Tag(static_cast<uint8_t>(state));
            animation["clip_start_frame"] = Tag(clip.startFrame);
            animation["clip_frame_count"] = Tag(clip.frameCount);
            animation["clip_frame_time"] = Tag(clip.frameTime);
            animation["clip_loop"] = Tag(clip.loop);

            animations_list.push_back(Tag(animation));
        }

        compound["animations"] = Tag(animations_list);

        compound["current_state"] = Tag(static_cast<uint32_t>(currentState));

        compound["direction"] = Tag(static_cast<bool>(direction));
        compound["timer"] = Tag(timer);
        compound["current_frame"] = Tag(currentFrame);
        compound["frame_size"] = Tag(TagCompound({{"x", Tag(frameSize.x)}, {"y", Tag(frameSize.y)}}));

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        animations.clear();

        for (const auto& animationTag : tag["animations"].get<TagList>())
        {
            const auto& animation = animationTag.get<TagCompound>();

            uint32_t raw_state = animation.at("state").get<uint8_t>();
            if (raw_state > static_cast<uint32_t>(AnimationState::Flying)) continue;

            AnimationState state = static_cast<AnimationState>(raw_state);

            AnimationClip clip
            {
                animation.at("clip_start_frame").get<uint32_t>(),
                animation.at("clip_frame_count").get<uint32_t>(),
                animation.at("clip_frame_time").get<float>(),
                animation.at("clip_loop").get<bool>()
            };

            animations[state] = clip;
        }

        uint32_t raw_current = tag["current_state"].get<uint32_t>();
        currentState = (raw_current <= static_cast<uint32_t>(AnimationState::Flying))
            ? static_cast<AnimationState>(raw_current)
            : AnimationState::Idle;

        direction = tag["direction"].get<bool>() ? Direction::Right : Direction::Left;

        timer = tag["timer"].get<float>();

        currentFrame = tag["current_frame"].get<uint32_t>();

        auto it = animations.find(currentState);
        if (it == animations.end() || it->second.frameCount == 0 || currentFrame >= it->second.frameCount)
        {
            currentFrame = 0;
            timer = 0.0f;
        }

        const auto& frameSizeTag = tag["frame_size"].get<TagCompound>();
        frameSize.x = frameSizeTag.at("x").get<int32_t>();
        frameSize.y = frameSizeTag.at("y").get<int32_t>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<AnimationComponent>(*this);
    }
};


#endif // ANIMATION_COMPONENT_HPP