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
    float available = size.y - (margin * 2.0f + spacing * static_cast<float>(elements.size() - 1));

    unsigned int total_mass = 0;

    for(auto& element : elements) total_mass += element->mass;

    float size_for_mass_unit =  available / static_cast<float>(total_mass);


    float indicator = position.y + margin;

    for(auto& element : elements)
    {
        float height = size_for_mass_unit * static_cast<float>(element->mass);

        sf::FloatRect bounds({position.x + margin, indicator}, {size.x - 2.0f * margin, height});

        element->setPosition(bounds.position);
        element->setSize(bounds.size);

        element->updateToBounds(bounds);

        indicator += spacing + bounds.size.y;
    }
}

void HorizontalStackLayout::updateToBounds(sf::FloatRect bounds)
{
    float available = size.x - (margin * 2.0f + spacing * static_cast<float>(elements.size() - 1));

    unsigned int total_mass = 0;

    for(auto& element : elements) total_mass += element->mass;

    float size_for_mass_unit =  available / static_cast<float>(total_mass);


    float indicator = position.x + margin;

    for(auto& element : elements)
    {
        float width = size_for_mass_unit * static_cast<float>(element->mass);

        sf::FloatRect bounds({indicator, position.y + margin}, {width, size.y - 2.0f * margin});

        element->setPosition(bounds.position);
        element->setSize(bounds.size);

        element->updateToBounds(bounds);

        indicator += spacing + bounds.size.x;
    }
}