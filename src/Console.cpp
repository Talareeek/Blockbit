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

        const float m = 8.0f;

        sf::FloatRect bounds = {position, size};
        if(!bounds.contains(mpos))
        {
            currentEdge = ResizeEdge::None;
            return;
        }

        bool onTop = mpos.y < position.y + m;
        bool onBottom = mpos.y > position.y + size.y - m;
        bool onLeft = mpos.x < position.x + m;
        bool onRight = mpos.x > position.x + size.x - m;

        if(onTop && onLeft) currentEdge = ResizeEdge::TopLeft;
        else if(onTop && onRight) currentEdge = ResizeEdge::TopRight;
        else if(onBottom && onLeft) currentEdge = ResizeEdge::BottomLeft;
        else if(onBottom && onRight) currentEdge = ResizeEdge::BottomRight;
        else if(onTop) currentEdge = ResizeEdge::Top;
        else if(onBottom) currentEdge = ResizeEdge::Bottom;
        else if(onLeft) currentEdge = ResizeEdge::Left;
        else if(onRight) currentEdge = ResizeEdge::Right;
        else
        {
            currentEdge = ResizeEdge::None;
            moving = true;
            lastMousePos = mpos;
            return;
        }

        dragging = true;
        lastMousePos = mpos;
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

        const float minSize = 80.0f;

        auto resizeTop = [&](float dy)
        {
            if(size.y - dy < minSize) dy = size.y - minSize;
            position.y += dy;
            size.y -= dy;
        };
        auto resizeBottom = [&](float dy)
        {
            if(size.y + dy < minSize) dy = minSize - size.y;
            size.y += dy;
        };
        auto resizeLeft = [&](float dx)
        {
            if(size.x - dx < minSize) dx = size.x - minSize;
            position.x += dx;
            size.x -= dx;
        };
        auto resizeRight = [&](float dx)
        {
            if(size.x + dx < minSize) dx = minSize - size.x;
            size.x += dx;
        };

        switch(currentEdge)
        {
            case ResizeEdge::Top:         resizeTop(delta.y); break;
            case ResizeEdge::Bottom:      resizeBottom(delta.y); break;
            case ResizeEdge::Left:        resizeLeft(delta.x); break;
            case ResizeEdge::Right:       resizeRight(delta.x); break;
            case ResizeEdge::TopLeft:     resizeTop(delta.y); resizeLeft(delta.x); break;
            case ResizeEdge::TopRight:    resizeTop(delta.y); resizeRight(delta.x); break;
            case ResizeEdge::BottomLeft:  resizeBottom(delta.y); resizeLeft(delta.x); break;
            case ResizeEdge::BottomRight: resizeBottom(delta.y); resizeRight(delta.x); break;
            default: break;
        }

        lastMousePos = mpos;
    }

    if(event.is<sf::Event::TextEntered>())
    {
        auto text = event.getIf<sf::Event::TextEntered>();

        if(text->unicode == L'\r')
        {
            writeLine(L"> " + incomming, sf::Color(170, 200, 230));

            std::wstringstream stream(incomming);

            std::wstring command;

            stream >> command;

            if(commandDatabase.contains(command))
            {
                if(commandDatabase[command].requires_world && world == nullptr) writeLine(L"No world assigned to console", sf::Color(235, 90, 90));
                else if (commandDatabase[command].requires_game && game == nullptr) writeLine(L"No game assigned to console", sf::Color(235, 90, 90));
                else
                {
                    try
                    {
                        commandDatabase[command].on_call(incomming, *this, game, world);
                    }
                    catch(std::exception& e)
                    {
                        writeLine(L"Command exception caught", sf::Color(235, 90, 90));
                    }

                }
            }
            else
            {
                writeLine(L"Unknown command: " + command, sf::Color(235, 90, 90));
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
    if(active) cursorTimer += dt;
}

void Console::render(sf::RenderWindow& window)
{
    if(!active) return;

    const sf::Color accent(80, 200, 220);
    const sf::Color bgColor(14, 18, 26, 230);
    const sf::Color titleBgColor(26, 32, 44, 245);
    const sf::Color separatorColor(60, 72, 92, 180);
    const sf::Color promptColor(120, 220, 140);

    sf::RectangleShape background(size);
    background.setPosition(position);
    background.setFillColor(bgColor);
    background.setOutlineThickness(2.0f);
    background.setOutlineColor(accent);
    window.draw(background);

    const float titleBarHeight = 22.0f;

    sf::RectangleShape titleBar({size.x, titleBarHeight});
    titleBar.setPosition(position);
    titleBar.setFillColor(titleBgColor);
    window.draw(titleBar);

    sf::RectangleShape titleSeparator({size.x, 1.0f});
    titleSeparator.setPosition({position.x, position.y + titleBarHeight});
    titleSeparator.setFillColor(accent);
    window.draw(titleSeparator);

    sf::Text title(AssetManager::getFont(1), L"Console", 13);
    title.setFillColor(accent);
    auto titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.position);
    title.setPosition({position.x + 8.0f, position.y + (titleBarHeight - titleBounds.size.y) * 0.5f - 1.0f});
    window.draw(title);

    for(int i = 0; i < 3; ++i)
    {
        sf::RectangleShape grip({2.0f, 2.0f});
        grip.setFillColor(accent);
        grip.setPosition({position.x + size.x - 4.0f - i * 3.0f, position.y + size.y - 4.0f});
        window.draw(grip);

        sf::RectangleShape grip2({2.0f, 2.0f});
        grip2.setFillColor(accent);
        grip2.setPosition({position.x + size.x - 4.0f, position.y + size.y - 4.0f - i * 3.0f});
        window.draw(grip2);
    }

    const float padding = 6.0f;
    float inputHeight = std::max(20.0f, std::min(28.0f, size.y * 0.08f));

    unsigned int inputCharSize = static_cast<unsigned int>(inputHeight - 8.0f);
    unsigned int logCharSize = static_cast<unsigned int>(inputCharSize * 0.95f);

    float inputAreaTop = position.y + size.y - inputHeight - padding;

    sf::RectangleShape inputSeparator({size.x - padding * 2, 1.0f});
    inputSeparator.setPosition({position.x + padding, inputAreaTop});
    inputSeparator.setFillColor(separatorColor);
    window.draw(inputSeparator);

    sf::Text prompt(AssetManager::getFont(1), L">", inputCharSize);
    prompt.setFillColor(promptColor);
    auto promptBounds = prompt.getLocalBounds();
    prompt.setOrigin(promptBounds.position);
    prompt.setPosition({position.x + padding + 2.0f, position.y + size.y - inputHeight + 2.0f});
    window.draw(prompt);

    float promptOffset = promptBounds.size.x + 8.0f;

    sf::Text input(AssetManager::getFont(1), incomming, inputCharSize);
    input.setFillColor(sf::Color::White);

    input.setPosition({position.x + padding + promptOffset, position.y + size.y - inputHeight + 2.0f});

    while(input.getLocalBounds().size.x > size.x - 2 * padding - promptOffset && !input.getString().isEmpty())
    {
        input.setString(input.getString().substring(1));
    }

    auto inputBounds = input.getLocalBounds();
    input.setOrigin(inputBounds.position);

    window.draw(input);

    if(static_cast<int>(cursorTimer * 2.0f) % 2 == 0)
    {
        sf::RectangleShape cursor({2.0f, static_cast<float>(inputCharSize)});
        cursor.setFillColor(sf::Color::White);
        cursor.setPosition({position.x + padding + promptOffset + inputBounds.size.x + 2.0f, position.y + size.y - inputHeight + 2.0f});
        window.draw(cursor);
    }

    float y = inputAreaTop - padding;
    float topLimit = position.y + titleBarHeight + padding;

    for(int i = static_cast<int>(logs.size()) - 1; i >= 0; --i)
    {
        const std::wstring& line = logs[i].text;
        const sf::Color& lineColor = logs[i].color;

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
            drawText.setFillColor(lineColor);
            drawText.setOutlineColor(sf::Color(0, 0, 0, 200));
            drawText.setOutlineThickness(1.0f);

            auto tb = drawText.getLocalBounds();
            float lineHeight = tb.size.y + 4.0f;

            y -= lineHeight;

            if(y < topLimit) return;

            drawText.setPosition({position.x + padding, y});
            drawText.setOrigin(tb.position);

            window.draw(drawText);
        }
    }
}


void Console::writeLine(std::wstring string)
{
    logs.push_back({std::move(string), sf::Color::White});
}

void Console::writeLine(std::wstring string, sf::Color color)
{
    logs.push_back({std::move(string), color});
}

void Console::assignWorld(World* world)
{
    this->world = world;
}

void Console::assignGame(Game* game)
{
    this->game = game;
}