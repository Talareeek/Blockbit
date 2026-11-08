#ifndef STAMINA_COMPONENT_HPP
#define STAMINA_COMPONENT_HPP

#include "Component.hpp"

#include <string>

struct StaminaComponent : public Component
{
    static constexpr float DEFAULT_MAX = 100.0f;
    static constexpr float REGEN_RATE = 25.0f;
    static constexpr float DRAIN_RATE = 15.0f;
    static constexpr float REGEN_DELAY = 1.0f;
    static constexpr float MIN_TO_START_SPRINT = 10.0f;

    float stamina = DEFAULT_MAX;
    float maxStamina = DEFAULT_MAX;
    float since_last_drain = REGEN_DELAY;

    bool sprint_requested = false;

    StaminaComponent() = default;
    StaminaComponent(float stamina, float maxStamina) : stamina(stamina), maxStamina(maxStamina) {}

    std::string name() const override
    {
        return "StaminaComponent";
    }

    Tag serialize() const override
    {
        TagCompound compound;

        compound["stamina"] = Tag(stamina);
        compound["max_stamina"] = Tag(maxStamina);

        return Tag(compound);
    }

    void deserialize(const Tag& tag) override
    {
        stamina = tag["stamina"].get<float>();
        maxStamina = tag["max_stamina"].get<float>();
    }

    std::unique_ptr<Component> clone() const override
    {
        return std::make_unique<StaminaComponent>(*this);
    }
};

#endif // STAMINA_COMPONENT_HPP
