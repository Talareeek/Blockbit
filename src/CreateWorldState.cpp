#include "../include/CreateWorldState.hpp"
#include "../include/InputManager.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/MainGameState.hpp"

CreateWorldState::CreateWorldState(Game* game) : GameState(game)
{
    UIElement::ScreenRelative name_relative;
    name_relative.position = {50.0f, 50.0f};
    name_relative.size = {10.0f, 5.0f};
    name_relative.axis = UIElement::ScreenRelative::Axis::Y;

    name = InputField(InputField({100.0f, 100.0f}, {400.0f, 50.0f}), "Name", "World Name");
    seed = InputField(InputField({100.0f, 200.0f}, {400.0f, 50.0f}), "", "Seed");
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

            World world(name.getText(), path, (!seed_text.empty()) ? potential_seed : std::rand());

            Game* game = this->game;

            game->popState();

            game->pushState(std::make_unique<MainGameState>(game, world));
        }
    );
}

void CreateWorldState::handleEvent(const sf::Event& event)
{
    name.handleEvent(event);
    seed.handleEvent(event);
    create.handleEvent(event);
}

void CreateWorldState::update(float dt)
{
    name.updateScreenRelative(game->getWindow().getSize());

    name.update(dt);

    seed.updateScreenRelative(game->getWindow().getSize());

    seed.update(dt);

    create.updateScreenRelative(game->getWindow().getSize());

    create.update(dt);

    if(InputManager::isLazyKeyPressed(sf::Keyboard::Key::Escape))
    {
        game->popState();
    }
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

    name.render(window);

    seed.render(window);

    create.render(window);
}