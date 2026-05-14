#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "UIElement.hpp"
#include <vector>
#include <string>
#include "Command.hpp"

class Console : public UIElement
{
private:

    enum class ResizeEdge
    {
        None,
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    struct LogEntry
    {
        std::wstring text;
        sf::Color color;
    };

    ResizeEdge currentEdge = ResizeEdge::None;
    bool dragging = false;
    sf::Vector2f lastMousePos;

    bool moving = false;

    bool active = false;

    std::vector<LogEntry> logs;

    std::wstring incomming;

    float cursorTimer = 0.0f;

    World* world = nullptr;

    Game* game = nullptr;

public:

    using UIElement::UIElement;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    void writeLine(std::wstring string);
    void writeLine(std::wstring string, sf::Color color);


    void assignWorld(World* world);
    void assignGame(Game* game);
};

#endif // CONSOLE_HPP