#include "../include/CreateWorldState.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/ClientGameState.hpp"

CreateWorldState::CreateWorldState(Game* game) : GameState(game)
{
    quit = Button({0.0f, 0.0f}, {0.0f, 0.0f}, sf::Color(211, 211, 211), "X");



    UIElement::ScreenRelative name_relative;
    name_relative.position = {50.0f, 50.0f};
    name_relative.size = {10.0f, 5.0f};
    name_relative.mode = UIElement::ScreenRelative::ScaleMode::UniformByHeight;

    name = InputField(InputField({100.0f, 100.0f}, {400.0f, 50.0f}), "Name", "World Name");
    seed = InputField(InputField({100.0f, 200.0f}, {400.0f, 50.0f}), "", "Seed");
    flat = Checkbox({100.0f, 260.0f}, {400.0f, 30.0f}, false, "Flat");
    UIElement::ScreenRelative previewRelative{{0.40f, 0.05f}, {0.55f, 0.90f}, UIElement::ScreenRelative::ScaleMode::Stretch};
    preview = GenerationPreview(previewRelative, static_cast<unsigned int>(std::rand()));
    preview.updateScreenRelative(game->getWindow().getSize());

    create = Button({100.0f, 300.0f}, {200.0f, 50.0f}, sf::Color::Green, "Create",
        [this]()
        {
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

            World world(name.getText(), path, (!seed_text.empty()) ? potential_seed : std::rand(), properties);

            Game* game = this->game;

            game->popState();

            game->pushState(std::make_unique<ClientGameState>(game, std::move(world)));
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
        game->popState();
        return;
    }

    sf::Vector2u winSize = game->getWindow().getSize();
    float buttonSize = winSize.y / 20.0f - 20.0f;
    quit.setSize({buttonSize, buttonSize});
    quit.setPosition({winSize.x - buttonSize - 10.0f, 10.0f});

    quit.update(dt);

    name.updateScreenRelative(game->getWindow().getSize());

    name.update(dt);

    seed.updateScreenRelative(game->getWindow().getSize());

    seed.update(dt);

    flat.updateScreenRelative(game->getWindow().getSize());

    flat.update(dt);

    create.updateScreenRelative(game->getWindow().getSize());

    create.update(dt);

    preview.updateScreenRelative(game->getWindow().getSize());
    preview.update(dt);

    
}

void CreateWorldState::render(sf::RenderWindow& window)
{
    sf::Texture texture = AssetManager::getTexture(3);

    texture.setRepeated(true);

    sf::RectangleShape background({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().x)});

    background.setTexture(&texture);

    background.setTextureRect({{0, 0}, {480, 480}});

    background.setPosition({0.0f, 0.0f});

    window.draw(background);


    sf::RectangleShape header({static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y) / 20});

    header.setPosition({0.0f, 0.0f});

    header.setFillColor(sf::Color(211, 211, 211));

    header.setOutlineColor(sf::Color::Black);

    header.setOutlineThickness(5.0f);

    window.draw(header);

    
    sf::Text header_text(AssetManager::getFont(0), "Create world", header.getSize().y - 20.0f);

    header_text.setPosition({10.0f, 10.0f});

    header_text.setOutlineColor(sf::Color::Black);
    header_text.setOutlineThickness(2.0f);

    window.draw(header_text);

    quit.render(window);

    name.render(window);

    seed.render(window);

    flat.render(window);

    create.render(window);

    preview.render(window);
}