#include "../include/WorldEntry.hpp"
#include "../include/AssetManager.hpp"

#include <algorithm>

namespace
{
    constexpr float DELETE_AREA_FACTOR = 1.0f;
}

void drawFitText(sf::RenderWindow& window, const std::string& str, const sf::FloatRect& box, bool center, sf::Color fill, float outline_thickness, sf::Color outline)
{
    if(str.empty() || box.size.x <= 0.0f || box.size.y <= 0.0f) return;

    sf::Text txt(AssetManager::getFont(0), str, static_cast<unsigned int>(std::max(1.0f, box.size.y * 0.7f)));

    auto bounds = txt.getLocalBounds();
    if(bounds.size.x <= 0.0f || bounds.size.y <= 0.0f) return;

    float sx = (box.size.x * 0.92f) / bounds.size.x;
    float sy = (box.size.y * 0.78f) / bounds.size.y;
    float scale = std::min({1.0f, sx, sy});

    txt.setScale({scale, scale});
    txt.setFillColor(fill);
    txt.setOutlineThickness(outline_thickness);
    txt.setOutlineColor(outline);

    bounds = txt.getLocalBounds();

    if(center)
    {
        txt.setPosition({
            box.position.x + (box.size.x - bounds.size.x * scale) * 0.5f - bounds.position.x * scale,
            box.position.y + (box.size.y - bounds.size.y * scale) * 0.5f - bounds.position.y * scale
        });
    }
    else
    {
        txt.setPosition({
            box.position.x + box.size.x * 0.04f - bounds.position.x * scale,
            box.position.y + (box.size.y - bounds.size.y * scale) * 0.5f - bounds.position.y * scale
        });
    }

    window.draw(txt);
}

WorldEntry::WorldEntry(const std::string& name, const std::filesystem::path& path)
    : name(name), path(path)
{
}

const std::string& WorldEntry::getName() const     { return name; }
const std::filesystem::path& WorldEntry::getPath() const { return path; }
bool WorldEntry::wasPlayRequested() const   { return play_requested; }
bool WorldEntry::wasDeleteRequested() const { return delete_requested; }
void WorldEntry::clearRequests()
{
    play_requested = false;
    delete_requested = false;
}

void WorldEntry::handleEvent(const sf::Event& event)
{
    float delete_w = size.y * DELETE_AREA_FACTOR;

    sf::FloatRect play_area{position, {size.x - delete_w, size.y}};
    sf::FloatRect delete_area{{position.x + size.x - delete_w, position.y}, {delete_w, size.y}};

    if(event.is<sf::Event::MouseMoved>())
    {
        auto mouse = event.getIf<sf::Event::MouseMoved>();
        sf::Vector2f m{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};
        hovering_play   = play_area.contains(m);
        hovering_delete = delete_area.contains(m);
    }
    else if(event.is<sf::Event::MouseButtonPressed>())
    {
        auto mouse = event.getIf<sf::Event::MouseButtonPressed>();
        if(mouse->button != sf::Mouse::Button::Left) return;

        sf::Vector2f m{static_cast<float>(mouse->position.x), static_cast<float>(mouse->position.y)};

        if(delete_area.contains(m))
        {
            click_sound.setVolume(100);
            click_sound.play();
            delete_requested = true;
        }
        else if(play_area.contains(m))
        {
            click_sound.setVolume(100);
            click_sound.play();
            play_requested = true;
        }
    }
}

void WorldEntry::update(float dt)
{
}

void WorldEntry::render(sf::RenderWindow& window)
{
    float delete_w = size.y * DELETE_AREA_FACTOR;

    sf::Color base_play    (55, 80, 55);
    sf::Color hover_play   (75, 110, 75);
    sf::Color base_delete  (140, 45, 45);
    sf::Color hover_delete (185, 60, 60);
    sf::Color outline_col  (255, 255, 255, 90);

    sf::RectangleShape frame(size);
    frame.setPosition(position);
    frame.setFillColor(hovering_play ? hover_play : base_play);
    frame.setOutlineThickness(2.0f);
    frame.setOutlineColor(outline_col);
    window.draw(frame);

    sf::Vector2f del_size{delete_w, size.y};
    sf::Vector2f del_pos{position.x + size.x - delete_w, position.y};

    sf::RectangleShape del(del_size);
    del.setPosition(del_pos);
    del.setFillColor(hovering_delete ? hover_delete : base_delete);
    window.draw(del);

    sf::RectangleShape divider({2.0f, size.y});
    divider.setPosition({del_pos.x, position.y});
    divider.setFillColor(outline_col);
    window.draw(divider);

    sf::FloatRect name_box{
        {position.x + size.x * 0.04f, position.y},
        {size.x - delete_w - size.x * 0.04f - 4.0f, size.y}
    };

    drawFitText(window, name, name_box, false, sf::Color::White, 1.0f, sf::Color::Black);
    drawFitText(window, "X", {del_pos, del_size}, true, sf::Color::White, 1.0f, sf::Color::Black);
}
