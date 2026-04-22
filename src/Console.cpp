#include "../include/Console.hpp"
#include "../include/InputManager.hpp"

void Console::handleEvent(const sf::Event& event)
{
    if(!active) return;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();

        sf::Vector2f mpos(mouse->position);

        sf::FloatRect top = {position, {size.x, 5.0f}};
        sf::FloatRect bottom = {{position.x, position.y + size.y - 5.0f}, {size.x, 5.0f}};
        sf::FloatRect left = {position, {5.0f, size.y}};
        sf::FloatRect right = {{position.x + size.x - 5.0f, position.y}, {5.0f, size.y}};
        sf::FloatRect body = {position + sf::Vector2f(5.0f, 5.0f), size - sf::Vector2f(10.0f, 10.0f)};

        if(top.contains(mpos)) currentEdge = ResizeEdge::Top;
        else if(bottom.contains(mpos)) currentEdge = ResizeEdge::Bottom;
        else if(left.contains(mpos)) currentEdge = ResizeEdge::Left;
        else if(right.contains(mpos)) currentEdge = ResizeEdge::Right;
        else if(body.contains(mpos))
        {
            moving = true;
            lastMousePos = mpos;
            return;
        }
        else currentEdge = ResizeEdge::None;

        if(currentEdge != ResizeEdge::None)
        {
            dragging = true;
            lastMousePos = mpos;
        }
    }

    if(event.is<sf::Event::MouseButtonReleased>())
    {
        dragging = false;
        moving = false;
        currentEdge = ResizeEdge::None;
    }

    if(event.is<sf::Event::MouseMoved>())
    {
        if(!dragging && !moving) return;

        if(moving)
        {
            auto mouse = event.getIf<sf::Event::MouseMoved>();
            sf::Vector2f mpos(mouse->position);

            sf::Vector2f delta = mpos - lastMousePos;

            position += delta;

            lastMousePos = mpos;
            return;
        }

        auto mouse = event.getIf<sf::Event::MouseMoved>();
        sf::Vector2f mpos(mouse->position);

        sf::Vector2f delta = mpos - lastMousePos;

        switch(currentEdge)
        {
            case ResizeEdge::Top:
                position.y += delta.y;
                size.y -= delta.y;
                break;

            case ResizeEdge::Bottom:
                size.y += delta.y;
                break;

            case ResizeEdge::Left:
                position.x += delta.x;
                size.x -= delta.x;
                break;

            case ResizeEdge::Right:
                size.x += delta.x;
                break;

            default:
                break;
        }

        size.x = std::max(size.x, 50.0f);
        size.y = std::max(size.y, 50.0f);

        lastMousePos = mpos;
    }
}

void Console::update(float dt)
{
    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Grave)) active = !active;
}

void Console::render(sf::RenderWindow& window)
{
    if(!active) return;

    sf::RectangleShape background(size - sf::Vector2f(10.0f, 10.0f));
    background.setPosition(position + sf::Vector2f(5.0f, 5.0f));

    sf::Color fill_color = sf::Color::Black;
    fill_color.a = 192;
    background.setFillColor(fill_color);

    background.setOutlineThickness(5.0f);
    background.setOutlineColor(sf::Color::Black);

    window.draw(background);
}