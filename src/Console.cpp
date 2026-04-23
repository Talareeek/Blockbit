#include "../include/Console.hpp"
#include "../include/InputManager.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Command.hpp"

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

    if(event.is<sf::Event::TextEntered>())
    {
        auto text = event.getIf<sf::Event::TextEntered>();

        if(text->unicode == L'\r')
        {
            logs.push_back(L"> " + incomming);

            std::wstringstream stream(incomming);

            std::wstring command;

            stream >> command;

            if(commandDatabase.contains(command))
            {
                if(!commandDatabase[command].requires_game && !commandDatabase[command].requires_world)
                {
                    commandDatabase[command].on_call(incomming, *this, nullptr, nullptr);
                }
            }

            incomming.clear();
        }
        else if(text->unicode == 8)
        {
            if(!incomming.empty()) incomming.pop_back();
        }
        else
        {
            incomming += text->unicode;
        }
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

    float padding = 5.0f;
    float inputHeight = size.y * 0.1f;
    float logAreaHeight = size.y - inputHeight - padding * 3;

    unsigned int inputCharSize = static_cast<unsigned int>(inputHeight - 10.0f);
    unsigned int logCharSize = static_cast<unsigned int>(inputCharSize * 0.75f);

    sf::Text input(AssetManager::getFont(1), incomming, inputCharSize);
    input.setFillColor(sf::Color::White);
    input.setOutlineColor(sf::Color::Black);
    input.setOutlineThickness(1.0f);

    input.setPosition({position.x + padding, position.y + size.y - inputHeight});

    while(input.getLocalBounds().size.x > size.x - 2 * padding && !input.getString().isEmpty())
    {
        input.setString(input.getString().substring(1));
    }

    auto bounds = input.getLocalBounds();
    input.setOrigin(bounds.position);

    window.draw(input);

    float y = position.y + size.y - inputHeight - padding;

    for(int i = static_cast<int>(logs.size()) - 1; i >= 0; --i)
    {
        std::wstring line = logs[i];
        sf::Text text(AssetManager::getFont(1), L"", logCharSize);

        std::vector<std::wstring> wrappedLines;
        std::wstring current;

        for(wchar_t c : line)
        {
            current += c;
            text.setString(current);

            if(text.getLocalBounds().size.x > size.x - 2 * padding)
            {
                current.pop_back();
                wrappedLines.push_back(current);
                current = c;
            }
        }

        if(!current.empty()) wrappedLines.push_back(current);

        for(int j = static_cast<int>(wrappedLines.size()) - 1; j >= 0; --j)
        {
            sf::Text drawText(AssetManager::getFont(1), wrappedLines[j], logCharSize);
            drawText.setFillColor(sf::Color::White);
            drawText.setOutlineColor(sf::Color::Black);
            drawText.setOutlineThickness(1.0f);

            auto tb = drawText.getLocalBounds();
            float lineHeight = tb.size.y + 4.0f;

            y -= lineHeight;

            if(y < position.y + padding) return;

            drawText.setPosition({position.x + padding, y});
            drawText.setOrigin(tb.position);

            window.draw(drawText);
        }
    }
}


void Console::writeLine(std::wstring string)
{
    logs.push_back(string);
}