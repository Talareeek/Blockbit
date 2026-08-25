#include "../include/WorldList.hpp"
#include "../include/Button.hpp"
#include "../include/World.hpp"
#include "../include/AssetManager.hpp"
#include "../include/ClientGameState.hpp"
#include "../include/CreateWorldState.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
    constexpr float STRIP_WIDTH_FACTOR        = 0.1f;
    constexpr float TAB_BAR_HEIGHT_FACTOR     = 0.07f;
    constexpr float HEADER_HEIGHT_FACTOR      = 0.05f;
    constexpr float CREATE_BUTTON_HEIGHT_FACTOR = 0.07f;
    constexpr float CREATE_BUTTON_GAP_FACTOR    = 0.02f;
    constexpr float ENTRY_HEIGHT_FACTOR       = 0.22f;
    constexpr float ENTRY_GAP_FACTOR          = 0.04f;
    constexpr float HORIZONTAL_PADDING_FACTOR = 0.04f;
    constexpr float SCROLL_STEP               = 40.0f;
}

WorldList::WorldList(std::filesystem::path path, Game* game) : game{game}, path(path)
{
    loadEntries();

    ipField      = InputField(InputField({0.0f, 0.0f}, {0.0f, 0.0f}), "", "Server IP");
    connectButton = Button({0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color(55, 90, 130), "Connect");

    createButton = Button({0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color(60, 140, 70), "Create World");
}

void WorldList::loadEntries()
{
    entries.clear();

    if(!std::filesystem::exists(path)) return;

    for(const auto& entry : std::filesystem::directory_iterator(path))
    {
        if(!entry.is_directory()) continue;

        std::filesystem::path manifest = entry.path() / "manifest";
        if(!std::filesystem::exists(manifest)) continue;

        std::ifstream file(manifest);
        std::string name;
        std::getline(file, name);

        entries.emplace_back(name, entry.path());
    }
}

sf::FloatRect WorldList::getTabBarArea() const
{
    float strip_w = size.x * STRIP_WIDTH_FACTOR;
    float bar_h   = size.y * TAB_BAR_HEIGHT_FACTOR;
    return sf::FloatRect({position.x + strip_w, position.y}, {size.x - strip_w, bar_h});
}

sf::FloatRect WorldList::getListArea() const
{
    float strip_w   = size.x * STRIP_WIDTH_FACTOR;
    float bar_h     = size.y * TAB_BAR_HEIGHT_FACTOR;
    float header_h  = size.y * HEADER_HEIGHT_FACTOR;
    float create_h  = size.y * CREATE_BUTTON_HEIGHT_FACTOR;
    float create_g  = size.y * CREATE_BUTTON_GAP_FACTOR;
    float top       = position.y + bar_h + header_h + create_h + create_g;
    float padding   = size.x * HORIZONTAL_PADDING_FACTOR;
    return sf::FloatRect({position.x + strip_w + padding, top}, {size.x - strip_w - 2.0f * padding, position.y + size.y - top - padding});
}

sf::FloatRect WorldList::getCreateButtonArea() const
{
    float strip_w  = size.x * STRIP_WIDTH_FACTOR;
    float bar_h    = size.y * TAB_BAR_HEIGHT_FACTOR;
    float header_h = size.y * HEADER_HEIGHT_FACTOR;
    float create_h = size.y * CREATE_BUTTON_HEIGHT_FACTOR;
    float padding  = size.x * HORIZONTAL_PADDING_FACTOR;
    return sf::FloatRect({position.x + strip_w + padding, position.y + bar_h + header_h}, {size.x - strip_w - 2.0f * padding, create_h});
}

sf::FloatRect WorldList::getServerPreviewArea() const
{
    sf::FloatRect list_area = getListArea();

    float strip_w = size.x * STRIP_WIDTH_FACTOR;
    float content_w = size.x - strip_w;
    float field_w = content_w * 0.85f;
    float field_h = size.y * 0.06f;
    float field_x = position.x + strip_w + (content_w - field_w) * 0.5f;
    float field_y = list_area.position.y + size.y * 0.04f;
    float button_h = field_h * 1.2f;
    float button_y = field_y + field_h + size.y * 0.025f;

    float top = button_y + button_h + size.y * 0.03f;
    float bottom = position.y + size.y - size.y * 0.04f;
    float height = std::max(0.0f, bottom - top);

    return sf::FloatRect({field_x, top}, {field_w, height});
}

void WorldList::handleEvent(const sf::Event& event)
{
    if(event.is<sf::Event::MouseMoved>())
    {
        auto mouse = event.getIf<sf::Event::MouseMoved>();
        sf::Vector2f mouse_pos{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};

        if(mode == Mode::HIDDEN)
        {
            sf::FloatRect sidebar{{position.x + size.x - size.x / 10.0f, 0.0f}, {size.x / 10.0f, size.y}};
            if(sidebar.contains(mouse_pos))
            {
                mode = Mode::ANIMATION;
                direction = AnimationDirection::LEFT;
            }
        }
        else if(mode == Mode::VISIBLE)
        {
            sf::FloatRect sidebar{position, size};
            if(!sidebar.contains(mouse_pos))
            {
                mode = Mode::ANIMATION;
                direction = AnimationDirection::RIGHT;
            }
        }
    }

    if(mode != Mode::VISIBLE) return;

    if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();
        if(mouse->button == sf::Mouse::Button::Left)
        {
            sf::FloatRect tab_bar = getTabBarArea();
            sf::Vector2f mouse_pos{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};

            if(tab_bar.contains(mouse_pos))
            {
                float half_x = tab_bar.position.x + tab_bar.size.x * 0.5f;

                Selection prev = selection;
                if(mouse_pos.x < half_x) selection = Selection::SINGLEPLAYER;
                else                     selection = Selection::MULTIPLAYER;

                if(prev != selection) scroll_offset = 0.0f;
            }
        }
    }

    if(selection == Selection::SINGLEPLAYER)
    {
        if(event.is<sf::Event::MouseWheelScrolled>())
        {
            auto wheel = event.getIf<sf::Event::MouseWheelScrolled>();
            sf::Vector2f mouse_pos{static_cast<float>(wheel->position.x), static_cast<float>(wheel->position.y)};

            sf::FloatRect list_area = getListArea();
            if(list_area.contains(mouse_pos))
            {
                scroll_offset -= wheel->delta * SCROLL_STEP;
            }
        }

        createButton.handleEvent(event);

        for(auto& entry : entries)
        {
            entry.handleEvent(event);
        }
    }
    else
    {
        ipField.handleEvent(event);
        connectButton.handleEvent(event);
    }
}

void WorldList::update(float dt)
{
    if(mode == Mode::ANIMATION)
    {
        animation_time += dt;
        if(animation_time >= ANIMATION_TOTAL_LENGTH)
        {
            animation_time = 0.0f;

            switch(direction)
            {
            case AnimationDirection::LEFT:  mode = Mode::VISIBLE; break;
            case AnimationDirection::RIGHT: mode = Mode::HIDDEN;  break;
            }
        }
    }

    sf::FloatRect list_area = getListArea();

    sf::FloatRect create_area = getCreateButtonArea();
    createButton.setPosition(create_area.position);
    createButton.setSize(create_area.size);

    if(selection == Selection::SINGLEPLAYER && createButton.clicked())
    {
        createButton.update(dt);
        game->pushState(&game->currentState(), std::make_unique<CreateWorldState>(game));
        return;
    }

    createButton.update(dt);

    float entry_h = size.x * ENTRY_HEIGHT_FACTOR;
    float gap     = size.x * ENTRY_GAP_FACTOR;

    content_height = entries.empty() ? 0.0f : entries.size() * entry_h + (entries.size() - 1) * gap;

    float max_scroll = std::max(0.0f, content_height - list_area.size.y);
    scroll_offset = std::clamp(scroll_offset, 0.0f, max_scroll);

    for(std::size_t i = 0; i < entries.size(); ++i)
    {
        auto& entry = entries[i];

        float y = list_area.position.y + i * (entry_h + gap) - scroll_offset;

        entry.setPosition({list_area.position.x, y});
        entry.setSize({list_area.size.x, entry_h});

        entry.update(dt);
    }

    std::vector<std::size_t> to_delete;
    for(std::size_t i = 0; i < entries.size(); ++i)
    {
        if(entries[i].wasPlayRequested())
        {
            auto world_path = entries[i].getPath();
            entries[i].clearRequests();
            game->pushState(&game->currentState(), std::make_unique<ClientGameState>(game, world_path, 0, nickname));
            return;
        }
        if(entries[i].wasHostRequested())
        {
            auto world_path = entries[i].getPath();
            entries[i].clearRequests();
            game->pushState(&game->currentState(), std::make_unique<ClientGameState>(game, world_path, ClientGameState::DEFAULT_PORT, nickname));
            return;
        }
        if(entries[i].wasDeleteRequested())
        {
            to_delete.push_back(i);
        }
    }

    for(auto it = to_delete.rbegin(); it != to_delete.rend(); ++it)
    {
        std::error_code ec;
        std::filesystem::remove_all(entries[*it].getPath(), ec);
        if(ec) std::cerr << "Failed to delete world: " << ec.message() << '\n';
        entries.erase(entries.begin() + *it);
    }

    if (pendingConnect.has_value())
    {
        auto [host, port] = *pendingConnect;
        pendingConnect.reset();
        try
        {
            game->pushState(&game->currentState(), std::make_unique<ClientGameState>(game, host, port, nickname));
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[Multiplayer] bad_alloc constructing ClientGameState\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Multiplayer] exception constructing ClientGameState: " << e.what() << '\n';
        }
        return;
    }

    float strip_w = size.x * STRIP_WIDTH_FACTOR;
    float content_w = size.x - strip_w;
    float field_w = content_w * 0.85f;
    float field_h = size.y * 0.06f;
    float field_x = position.x + strip_w + (content_w - field_w) * 0.5f;
    float field_y = list_area.position.y + size.y * 0.04f;

    ipField.setPosition({field_x, field_y});
    ipField.setSize({field_w, field_h});

    connectButton.setPosition({field_x, field_y + field_h + size.y * 0.025f});
    connectButton.setSize({field_w, field_h * 1.2f});

    if (connectButton.clicked() && !pendingConnect.has_value())
    {
        std::string text = ipField.getText();
        if (!text.empty())
        {
            std::string host = text;
            uint16_t port = GameServer::DEFAULT_PORT;

            auto colon = text.find(':');
            if (colon != std::string::npos)
            {
                host = text.substr(0, colon);
                std::string port_str = text.substr(colon + 1);
                try { port = static_cast<uint16_t>(std::stoi(port_str)); }
                catch (...) { port = GameServer::DEFAULT_PORT; }
            }

            std::cerr << "[Multiplayer] Connect to: " << host << ':' << port << '\n';
            pendingConnect = std::make_pair(host, port);
        }
    }

    ipField.update(dt);
    connectButton.update(dt);
}

void WorldList::render(sf::RenderWindow& window)
{
    sf::Texture& background_texture = AssetManager::getUITexture(AssetManager::UITextureID::UIBackground);
    background_texture.setRepeated(true);

    sf::Vector2u texSize = background_texture.getSize();

    auto makeTexRect = [&](sf::Vector2f areaSize)
    {
        return sf::IntRect({0, 0}, {static_cast<int>(areaSize.x / texSize.x * texSize.x), static_cast<int>(areaSize.y / texSize.y * texSize.y)});
    };

    auto drawTexturedPanel = [&](sf::Vector2f pos, sf::Vector2f sz, sf::Color overlay)
    {
        sf::RectangleShape bg(sz);
        bg.setPosition(pos);
        bg.setTexture(&background_texture);
        bg.setTextureRect(makeTexRect(sz));
        window.draw(bg);

        sf::RectangleShape ov(sz);
        ov.setPosition(pos);
        ov.setFillColor(overlay);
        window.draw(ov);
    };

    if(mode == Mode::HIDDEN)
    {
        sf::Vector2f sidebar_size{size.x / 10.0f, size.y};
        sf::Vector2f sidebar_pos{position.x + size.x - sidebar_size.x, 0.0f};

        drawTexturedPanel(sidebar_pos, sidebar_size, sf::Color(0, 0, 0, 150));

        sf::FloatRect arrow_box{sidebar_pos, {sidebar_size.x, sidebar_size.x}};
        arrow_box.position.y = sidebar_pos.y + (sidebar_size.y - arrow_box.size.y) * 0.5f;
        drawFitText(window, "<", arrow_box, true, sf::Color(230, 230, 230), 1.0f, sf::Color::Black);
        return;
    }

    if(mode == Mode::ANIMATION)
    {
        float t = animation_time / ANIMATION_TOTAL_LENGTH;
        float width = (direction == AnimationDirection::LEFT)
                      ? size.x / 10.0f + t * (size.x * 0.9f)
                      : size.x - t * (size.x * 0.9f);

        sf::Vector2f anim_size{width, size.y};
        sf::Vector2f anim_pos{position.x + size.x - width, 0.0f};

        drawTexturedPanel(anim_pos, anim_size, sf::Color(0, 0, 0, 150));
        return;
    }

    drawTexturedPanel(position, size, sf::Color(0, 0, 0, 120));

    sf::FloatRect tab_bar = getTabBarArea();
    float half_w = tab_bar.size.x * 0.5f;

    auto drawTab = [&](sf::Vector2f tab_pos, sf::Vector2f tab_size, const std::string& label, bool active)
    {
        sf::RectangleShape tab(tab_size);
        tab.setPosition(tab_pos);
        tab.setFillColor(active ? sf::Color(0, 0, 0, 60) : sf::Color(0, 0, 0, 160));
        window.draw(tab);

        drawFitText(window, label, sf::FloatRect(tab_pos, tab_size), true,
                    active ? sf::Color(255, 255, 255) : sf::Color(200, 200, 200),
                    1.0f, sf::Color::Black);

        if(active)
        {
            sf::RectangleShape underline({tab_size.x * 0.6f, tab_size.y * 0.07f});
            underline.setPosition({tab_pos.x + tab_size.x * 0.2f, tab_pos.y + tab_size.y - underline.getSize().y});
            underline.setFillColor(sf::Color(120, 200, 255));
            window.draw(underline);
        }
    };

    drawTab({tab_bar.position.x, tab_bar.position.y}, {half_w, tab_bar.size.y}, "Worlds",      selection == Selection::SINGLEPLAYER);
    drawTab({tab_bar.position.x + half_w, tab_bar.position.y}, {half_w, tab_bar.size.y}, "Multiplayer", selection == Selection::MULTIPLAYER);

    sf::RectangleShape separator({tab_bar.size.x, 2.0f});
    separator.setPosition({tab_bar.position.x, tab_bar.position.y + tab_bar.size.y});
    separator.setFillColor(sf::Color(255, 255, 255, 60));
    window.draw(separator);

    float strip_w_local = size.x * STRIP_WIDTH_FACTOR;
    sf::FloatRect header_box{
        {position.x + strip_w_local, position.y + tab_bar.size.y},
        {size.x - strip_w_local, size.y * HEADER_HEIGHT_FACTOR}
    };

    if(selection == Selection::SINGLEPLAYER)
    {
        sf::FloatRect list_area = getListArea();

        std::string header_text = entries.empty() ? "No worlds yet" : "Select a world";
        drawFitText(window, header_text, header_box, true, sf::Color(220, 220, 220), 1.0f, sf::Color::Black);

        createButton.render(window);

        if(!entries.empty())
        {
            sf::Vector2u win_size = window.getSize();
            sf::View saved_view = window.getView();

            sf::View clip_view(sf::FloatRect({list_area.position.x, list_area.position.y}, {list_area.size.x, list_area.size.y}));
            clip_view.setViewport(sf::FloatRect(
                {list_area.position.x / win_size.x, list_area.position.y / win_size.y},
                {list_area.size.x / win_size.x, list_area.size.y / win_size.y}
            ));
            window.setView(clip_view);

            for(auto& entry : entries)
            {
                sf::FloatRect ebounds{entry.getPosition(), entry.getSize()};
                if(ebounds.findIntersection(list_area))
                {
                    entry.render(window);
                }
            }

            window.setView(saved_view);

            if(content_height > list_area.size.y)
            {
                float track_w = 4.0f;
                float track_x = list_area.position.x + list_area.size.x - track_w;

                sf::RectangleShape track({track_w, list_area.size.y});
                track.setPosition({track_x, list_area.position.y});
                track.setFillColor(sf::Color(0, 0, 0, 80));
                window.draw(track);

                float thumb_h = list_area.size.y * (list_area.size.y / content_height);
                float thumb_y = list_area.position.y + (scroll_offset / content_height) * list_area.size.y;

                sf::RectangleShape thumb({track_w, thumb_h});
                thumb.setPosition({track_x, thumb_y});
                thumb.setFillColor(sf::Color(200, 200, 200, 180));
                window.draw(thumb);
            }
        }
    }
    else
    {
        drawFitText(window, "Connect to server", header_box, true, sf::Color(220, 220, 220), 1.0f, sf::Color::Black);

        float label_h = ipField.getSize().y * 0.6f;
        sf::FloatRect label_box{
            {ipField.getPosition().x, ipField.getPosition().y - label_h - 4.0f},
            {ipField.getSize().x, label_h}
        };
        drawFitText(window, "Server IP", label_box, false, sf::Color(220, 220, 220), 1.0f, sf::Color::Black);

        sf::RectangleShape field_bg(ipField.getSize());
        field_bg.setPosition(ipField.getPosition());
        field_bg.setFillColor(sf::Color(20, 20, 20, 200));
        field_bg.setOutlineColor(sf::Color(120, 200, 255, 180));
        field_bg.setOutlineThickness(1.5f);
        window.draw(field_bg);

        ipField.render(window);
        connectButton.render(window);

        if(ipField.getText().empty())
        {
            drawFitText(window, "e.g. 127.0.0.1:34500",
                        sf::FloatRect(ipField.getPosition(), ipField.getSize()),
                        false, sf::Color(180, 180, 180, 140), 0.0f, sf::Color::Transparent);
        }
    }

    float strip_w = size.x * STRIP_WIDTH_FACTOR;

    sf::RectangleShape collapse_strip({strip_w, size.y});
    collapse_strip.setPosition({position.x, 0.0f});
    collapse_strip.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(collapse_strip);

    sf::RectangleShape divider({2.0f, size.y});
    divider.setPosition({position.x + strip_w, 0.0f});
    divider.setFillColor(sf::Color(255, 255, 255, 35));
    window.draw(divider);

    sf::FloatRect arrow_box{
        {position.x, position.y + (size.y - strip_w) * 0.5f},
        {strip_w, strip_w}
    };
    drawFitText(window, ">", arrow_box, true, sf::Color(230, 230, 230), 1.0f, sf::Color::Black);
}
