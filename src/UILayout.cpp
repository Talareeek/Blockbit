#include "../include/UILayout.hpp"

void UILayout::handleEvent(const sf::Event& event)
{
    for(auto& element : elements)
    {
        element->handleEvent(event);
    }
}

void UILayout::update(float dt)
{
    for(auto& element : elements)
    {
        element->update(dt);
    }
}

void UILayout::render(sf::RenderWindow& window)
{
    for(auto& element : elements)
    {
        element->render(window);
    }
}


void VerticalStackLayout::updateToBounds(sf::FloatRect bounds)
{
    float available = size.y -
        (margin * 2.0f + spacing * static_cast<float>(elements.size() - 1));

    unsigned int total_mass = 0;

    for (auto& element : elements)
    {
        if (element->mass > 0)
            total_mass += element->mass;
        else
            available -= element->size.y;
    }

    float size_for_mass_unit =
        total_mass > 0
        ? available / static_cast<float>(total_mass)
        : 0.0f;

    float indicator = position.y + margin;

    for (auto& element : elements)
    {
        float height;

        if (element->mass == 0)
            height = element->size.y;
        else
            height = size_for_mass_unit * static_cast<float>(element->mass);

        sf::FloatRect elementBounds(
            {position.x + margin, indicator},
            {size.x - 2.0f * margin, height}
        );

        element->position = elementBounds.position;
        element->size = elementBounds.size;
        element->updateToBounds(elementBounds);

        indicator += spacing + elementBounds.size.y;
    }
}

void HorizontalStackLayout::updateToBounds(sf::FloatRect bounds)
{
    float available = size.x -
        (margin * 2.0f + spacing * static_cast<float>(elements.size() - 1));

    unsigned int total_mass = 0;

    for (auto& element : elements)
    {
        if (element->mass > 0)
            total_mass += element->mass;
        else
            available -= element->size.x;
    }

    float size_for_mass_unit =
        total_mass > 0
        ? available / static_cast<float>(total_mass)
        : 0.0f;

    float indicator = position.x + margin;

    for (auto& element : elements)
    {
        float width;

        if (element->mass == 0)
            width = element->size.x;
        else
            width = size_for_mass_unit * static_cast<float>(element->mass);

        sf::FloatRect elementBounds(
            {indicator, position.y + margin},
            {width, size.y - 2.0f * margin}
        );

        element->position = elementBounds.position;
        element->size = elementBounds.size;
        element->updateToBounds(elementBounds);

        indicator += spacing + elementBounds.size.x;
    }
}