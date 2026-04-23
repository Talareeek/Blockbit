#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "UIElement.hpp"
#include <vector>
#include <string>

class Console : public UIElement
{
private:

    enum class ResizeEdge
    {
        None,
        Top,
        Bottom,
        Left,
        Right
    };

    ResizeEdge currentEdge = ResizeEdge::None;
    bool dragging = false;
    sf::Vector2f lastMousePos;

    bool moving = false;

    bool active = false;

    std::vector<std::wstring> logs; 

    std::wstring incomming;

public:

    using UIElement::UIElement;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void writeLine(std::wstring string);
};

#endif // CONSOLE_HPP