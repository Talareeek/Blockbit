#include "../include/CreateWorldState.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/ClientGameState.hpp"

namespace
{
    constexpr float HEADER_H     = 0.07f;

    constexpr float PANEL_X      = 0.03f;
    constexpr float PANEL_Y      = 0.10f;
    constexpr float PANEL_W      = 0.34f;
    constexpr float PANEL_H      = 0.85f;

    constexpr float FIELD_X      = PANEL_X + 0.02f;
    constexpr float FIELD_W      = PANEL_W - 0.04f;

    constexpr float NAME_Y       = 0.20f;
    constexpr float SEED_Y       = 0.34f;
    constexpr float FLAT_Y       = 0.48f;

    constexpr float FIELD_H      = 0.06f;
    constexpr float FLAT_H       = 0.05f;

    constexpr float CREATE_Y     = 0.85f;
    constexpr float CREATE_H     = 0.08f;

    constexpr float PREVIEW_X    = 0.40f;
    constexpr float PREVIEW_Y    = 0.12f;
    constexpr float PREVIEW_W    = 0.57f;
    constexpr float PREVIEW_H    = 0.83f;
}

CreateWorldState::CreateWorldState(Game* game) : GameState(game)
{
    UIElement::ScreenRelative quitRelative{
        {1.0f - 0.055f, 0.012f},
        {0.04f, 0.05f},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    quit = Button(quitRelative, sf::Color(200, 70, 70), "X");

    UIElement::ScreenRelative nameRelative{
        {FIELD_X, NAME_Y}, {FIELD_W, FIELD_H},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    UIElement::ScreenRelative seedRelative{
        {FIELD_X, SEED_Y}, {FIELD_W, FIELD_H},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    UIElement::ScreenRelative flatRelative{
        {FIELD_X, FLAT_Y}, {FIELD_W, FLAT_H},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    UIElement::ScreenRelative createRelative{
        {FIELD_X, CREATE_Y}, {FIELD_W, CREATE_H},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    name = InputField(InputField(nameRelative), "", "World name");
    seed = InputField(InputField(seedRelative), "", "Seed (empty = random)");
    flat = Checkbox(flatRelative, false, "Flat terrain");

    UIElement::ScreenRelative previewRelative{
        {PREVIEW_X, PREVIEW_Y}, {PREVIEW_W, PREVIEW_H},
        UIElement::ScreenRelative::ScaleMode::Stretch};

    preview = GenerationPreview(previewRelative, static_cast<unsigned int>(std::rand()));
    preview.updateScreenRelative(game->getWindow().getSize());

    create = Button(createRelative, sf::Color(70, 160, 90), "Create world",
        [this]()
        {
            if (name.getText().empty()) return;

            std::string home;

            #ifdef _WIN32
                const char* appdata = std::getenv("APPDATA");
                home = appdata ? appdata : "";
            #elif __linux__
                const char* homeenv = std::getenv("HOME");
                home = homeenv ? homeenv : "";
            #endif

            std::filesystem::path savesPath = home.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(home);
            savesPath /= "Blockbit";
            savesPath /= "saves";

            std::filesystem::path path = savesPath / name.getText();

            std::filesystem::create_directory(path);

            std::string seed_text = seed.getText();
            unsigned int potential_seed;

            if(!seed_text.empty())
            {
                std::istringstream stream(seed_text);
                stream >> potential_seed;
            }

            GenerationProperties properties;

            properties.flat = flat.getValue();

            properties.base_height = preview.getBaseHeight();
            properties.height_scale = preview.getHeightScale();
            properties.frequency = preview.getFrequency();
            properties.amplitude = preview.getAmplitude();
            properties.persistence = preview.getPersistence();

            this->game->popState(this);
            this->game->pushState(this, std::make_unique<ClientGameState>(this->game, name.getText(), (!seed_text.empty()) ? potential_seed : std::rand(), properties, 0, "Player"));
        }
    );
}

void CreateWorldState::handleEvent(const sf::Event& event)
{
    quit.handleEvent(event);
    name.handleEvent(event);
    seed.handleEvent(event);
    flat.handleEvent(event);
    create.handleEvent(event);
    preview.handleEvent(event);
}

void CreateWorldState::update(float dt)
{
    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Escape) || quit.clicked())
    {
        game->popState(this);
        return;
    }

    sf::Vector2u winSize = game->getWindow().getSize();

    quit.updateScreenRelative(winSize);
    name.updateScreenRelative(winSize);
    seed.updateScreenRelative(winSize);
    flat.updateScreenRelative(winSize);
    create.updateScreenRelative(winSize);
    preview.updateScreenRelative(winSize);

    quit.update(dt);
    name.update(dt);
    seed.update(dt);
    flat.update(dt);
    create.update(dt);
    preview.update(dt);
}

void CreateWorldState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f},
        {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    const float win_w = static_cast<float>(window.getSize().x);
    const float win_h = static_cast<float>(window.getSize().y);

    sf::RectangleShape overlay({win_w, win_h});
    overlay.setFillColor(sf::Color(10, 15, 25, 140));
    window.draw(overlay);

    sf::RectangleShape header({win_w, win_h * HEADER_H});
    header.setPosition({0.0f, 0.0f});
    header.setFillColor(sf::Color(30, 40, 60, 220));
    header.setOutlineColor(sf::Color(0, 0, 0, 180));
    header.setOutlineThickness(2.0f);
    window.draw(header);

    sf::Text header_text(AssetManager::getFont(AssetManager::FontID::PressStart2P), "Create world",
        static_cast<unsigned>(header.getSize().y * 0.55f));
    header_text.setFillColor(sf::Color::White);
    header_text.setOutlineColor(sf::Color::Black);
    header_text.setOutlineThickness(2.0f);
    {
        auto b = header_text.getLocalBounds();
        header_text.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        header_text.setPosition({win_w * 0.5f - b.size.x * 0.5f, header.getSize().y * 0.5f});
    }
    window.draw(header_text);

    sf::RectangleShape panel({win_w * PANEL_W, win_h * PANEL_H});
    panel.setPosition({win_w * PANEL_X, win_h * PANEL_Y});
    panel.setFillColor(sf::Color(20, 25, 40, 190));
    panel.setOutlineColor(sf::Color(80, 100, 130, 220));
    panel.setOutlineThickness(2.0f);
    window.draw(panel);

    sf::RectangleShape panel_header({win_w * PANEL_W, win_h * 0.045f});
    panel_header.setPosition({win_w * PANEL_X, win_h * PANEL_Y});
    panel_header.setFillColor(sf::Color(40, 55, 85, 230));
    window.draw(panel_header);

    sf::Text panel_title(AssetManager::getFont(AssetManager::FontID::PressStart2P), "World settings",
        static_cast<unsigned>(panel_header.getSize().y * 0.55f));
    panel_title.setFillColor(sf::Color(220, 230, 245));
    panel_title.setOutlineColor(sf::Color::Black);
    panel_title.setOutlineThickness(1.0f);
    {
        auto b = panel_title.getLocalBounds();
        panel_title.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        panel_title.setPosition({panel_header.getPosition().x + win_w * 0.012f,
                                 panel_header.getPosition().y + panel_header.getSize().y * 0.5f});
    }
    window.draw(panel_title);

    auto drawLabel = [&](const std::string& str, float rel_x, float rel_y)
    {
        sf::Text label(AssetManager::getFont(AssetManager::FontID::PressStart2P), str, static_cast<unsigned>(win_h * 0.022f));
        label.setFillColor(sf::Color(220, 225, 235));
        label.setOutlineColor(sf::Color::Black);
        label.setOutlineThickness(1.0f);
        auto b = label.getLocalBounds();
        label.setOrigin({b.position.x, b.position.y + b.size.y});
        label.setPosition({win_w * rel_x, win_h * rel_y});
        window.draw(label);
    };

    drawLabel("Name",  FIELD_X, NAME_Y - 0.005f);
    drawLabel("Seed",  FIELD_X, SEED_Y - 0.005f);
    drawLabel("Terrain preview", PREVIEW_X, PREVIEW_Y - 0.025f);

    name.render(window);
    seed.render(window);
    flat.render(window);
    create.render(window);
    preview.render(window);

    quit.render(window);
}
