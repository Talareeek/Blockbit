#include "../include/PauseScreenState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/ClientGameState.hpp"
#include "../include/Settings.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
    constexpr float SAVE_FEEDBACK_DURATION = 1.5f;
}

PauseScreenState::PauseScreenState(Game* game, ClientGameState* client_state) : GameState(game), client_state(client_state)
{
    if(!blockbit_logo.loadFromFile("resources/textures/blockbit_logo.png")) throw std::runtime_error("Failed loading resources/textures/blockbit_logo.png");

    has_local_server = client_state && client_state->isLocalSession();

    UIElement::ScreenRelative resumeRelative  {{0.015f, 0.72f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};
    UIElement::ScreenRelative saveRelative    {{0.015f, 0.77f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};
    UIElement::ScreenRelative settingsRelative{{0.015f, 0.82f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};
    UIElement::ScreenRelative leaveRelative   {{0.015f, 0.87f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};

    resume = TextButton(resumeRelative, "Resume", [this]()
    {
        this->game->popState(this);
    });
    resume.setColors(sf::Color(210, 210, 210), sf::Color(130, 230, 130));

    save_world = TextButton(saveRelative, "Save World", [this]()
    {
        if (!this->client_state) return;
        try
        {
            this->client_state->saveLocalWorld();
            save_feedback = true;
            save_feedback_timer = SAVE_FEEDBACK_DURATION;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Pause] Save world failed: " << exception.what() << '\n';
        }
    });
    save_world.setColors(sf::Color(210, 210, 210), sf::Color(130, 190, 255));

    settings = TextButton(settingsRelative, "Settings", [this]()
    {
        this->game->getSettingsWindow().show();
    });
    settings.setColors(sf::Color(210, 210, 210), Settings::ui.main_color);

    const std::string leaveLabel = has_local_server ? "Save and Leave World" : "Leave Server";
    leave = TextButton(leaveRelative, leaveLabel, [this]()
    {
        if (this->has_local_server && this->client_state)
        {
            try
            {
                this->client_state->saveLocalWorld();
            }
            catch (const std::exception& exception)
            {
                std::cerr << "[Pause] Save on leave failed: " << exception.what() << '\n';
            }
        }
        this->game->popStates(this, 2);
    });
    leave.setColors(sf::Color(210, 210, 210), sf::Color(230, 80, 80));

    auto size = game->getWindow().getSize();
    resume.updateScreenRelative(size);
    save_world.updateScreenRelative(size);
    settings.updateScreenRelative(size);
    leave.updateScreenRelative(size);
}

void PauseScreenState::handleEvent(const sf::Event& event)
{
    resume.handleEvent(event);
    if (has_local_server) save_world.handleEvent(event);
    settings.handleEvent(event);
    leave.handleEvent(event);
}

void PauseScreenState::update(float dt)
{
    auto size = game->getWindow().getSize();

    resume.updateScreenRelative(size);
    save_world.updateScreenRelative(size);
    settings.updateScreenRelative(size);
    settings.setColors(sf::Color(210, 210, 210), Settings::ui.main_color);
    leave.updateScreenRelative(size);

    resume.update(dt);
    if (has_local_server) save_world.update(dt);
    settings.update(dt);
    leave.update(dt);

    if (save_feedback)
    {
        save_feedback_timer -= dt;
        if (save_feedback_timer <= 0.0f) save_feedback = false;
    }
}

void PauseScreenState::render(sf::RenderWindow& window)
{
    window.setView(window.getDefaultView());

    sf::RectangleShape dim;
    dim.setPosition({0.0f, 0.0f});
    dim.setSize({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    dim.setFillColor(sf::Color(0, 0, 0, 110));
    window.draw(dim);

    const float sidebarWidth = window.getSize().x * 0.20f;
    const float windowHeight = static_cast<float>(window.getSize().y);

    sf::RectangleShape sidebar({sidebarWidth, windowHeight});
    sidebar.setPosition({0.0f, 0.0f});
    const sf::Color main = Settings::ui.main_color;
    sidebar.setFillColor(sf::Color(10, 12, 18, 220));
    sidebar.setOutlineThickness(2.0f);
    sidebar.setOutlineColor(sf::Color(main.r, main.g, main.b, 120));
    window.draw(sidebar);

    sf::RectangleShape sidebarEdge({3.0f, windowHeight});
    sidebarEdge.setPosition({sidebarWidth, 0.0f});
    sidebarEdge.setFillColor(sf::Color(main.r, main.g, main.b, 180));
    window.draw(sidebarEdge);

    sf::RectangleShape logo;
    logo.setTexture(&blockbit_logo);
    logo.setSize({static_cast<float>(blockbit_logo.getSize().x), static_cast<float>(blockbit_logo.getSize().y)});

    const float logoWidth = sidebarWidth * 0.80f;
    const float factor = logoWidth / static_cast<float>(blockbit_logo.getSize().x);
    logo.setScale({factor, factor});
    logo.setPosition({sidebarWidth * 0.10f, windowHeight * 0.03f});
    window.draw(logo);

    sf::Text paused(AssetManager::getFont(AssetManager::FontID::PressStart2P), "Paused", 20);
    sf::FloatRect pausedBounds = paused.getLocalBounds();
    paused.setOrigin({pausedBounds.position.x, pausedBounds.position.y});
    paused.setPosition({sidebarWidth * 0.10f, windowHeight * 0.60f});
    paused.setFillColor(main);
    paused.setOutlineThickness(1.0f);
    paused.setOutlineColor(sf::Color(0, 0, 0, 180));
    window.draw(paused);

    resume.render(window);
    if (has_local_server) save_world.render(window);
    settings.render(window);
    leave.render(window);

    if (save_feedback)
    {
        sf::Text feedback(AssetManager::getFont(AssetManager::FontID::PressStart2P), "World Saved", 14);
        sf::FloatRect fb = feedback.getLocalBounds();
        feedback.setOrigin({fb.position.x, fb.position.y});
        feedback.setPosition({sidebarWidth * 0.10f, windowHeight * 0.93f});

        float alpha = std::min(1.0f, save_feedback_timer / 0.4f) * 255.0f;
        feedback.setFillColor(sf::Color(130, 230, 130, static_cast<std::uint8_t>(alpha)));
        feedback.setOutlineThickness(1.0f);
        feedback.setOutlineColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(alpha * 0.7f)));
        window.draw(feedback);
    }
}
