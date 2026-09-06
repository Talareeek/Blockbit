#include "../include/MenuGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Client.hpp"
#include "../include/Render.hpp"

#include <chrono>
#include <iostream>
#include <utility>

namespace
{
    constexpr float IP_DEBOUNCE_SECONDS = 0.5f;
    constexpr auto  PROBE_CONNECT_TIMEOUT = std::chrono::milliseconds(1500);
    constexpr auto  PROBE_RESPONSE_TIMEOUT = std::chrono::milliseconds(2500);

    bool parseHostPort(const std::string& text, std::string& host, uint16_t& port)
    {
        if (text.empty()) return false;

        host = text;
        port = 34500;

        auto colon = text.find(':');
        if (colon != std::string::npos)
        {
            host = text.substr(0, colon);
            std::string port_str = text.substr(colon + 1);
            try
            {
                int parsed = std::stoi(port_str);
                if (parsed <= 0 || parsed > 65535) return false;
                port = static_cast<uint16_t>(parsed);
            }
            catch (...)
            {
                return false;
            }
        }

        return !host.empty();
    }
}

MenuGameState::MenuGameState(Game* game) : GameState(game), serverPreview(&statusPacket)
{
    if(!blockbit_logo.loadFromFile("resources/textures/blockbit_logo_white.png")) throw std::runtime_error("Failed loading resources/textures/blockbit_logo_white.png");


    UIElement::ScreenRelative quitRelative{{0.01f, 0.92f}, {0.12f, 0.06f}, UIElement::ScreenRelative::ScaleMode::UniformByHeight};

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

    std::filesystem::create_directories(savesPath);
    worldList = WorldList(savesPath, game);

    quit = Button(quitRelative, sf::Color::Red, "Quit", [this]()
    {
        this->game->popState(this);
    });

    quit.updateScreenRelative(game->getWindow().getSize());

    UIElement::ScreenRelative sliderRelative{{0.30f, 0.45f}, {0.30f, 0.08f}, UIElement::ScreenRelative::ScaleMode::UniformByHeight};

    nicknameField = InputField(InputField({0.0f, 0.0f}, {0.0f, 0.0f}), "Player", "Nickname");

    backgroundTexture = generateBackground();

    sf::Music background_music = std::move(AssetManager::getMusic(AssetManager::MusicID::Dream));

    background_music.setVolume(25.0f);

    background_music.setLooping(true);

    game->jukebox.playMusic(background_music);
}

MenuGameState::~MenuGameState()
{
    cancelActiveProbe();
    if (probeThread.joinable()) probeThread.detach();

    game->jukebox.reset();
}

void MenuGameState::cancelActiveProbe()
{
    if (activeProbe) activeProbe->cancelled = true;
}

void MenuGameState::beginProbe(const std::string& text)
{
    std::string host;
    uint16_t port = 0;
    if (!parseHostPort(text, host, port)) return;

    cancelActiveProbe();
    if (probeThread.joinable()) probeThread.detach();

    auto state = std::make_shared<ProbeState>();
    activeProbe = state;

    probeThread = std::thread([state, host, port]()
    {
        try
        {
            Client client;

            if (!client.connect(host, port, PROBE_CONNECT_TIMEOUT) || state->cancelled)
            {
                client.disconnect();
                state->done = true;
                return;
            }

            client.send(serializePacket(StatusRequestPacket{}));

            auto deadline = std::chrono::steady_clock::now() + PROBE_RESPONSE_TIMEOUT;
            while (!state->cancelled && client.isConnected() && std::chrono::steady_clock::now() < deadline)
            {
                auto packets = client.poll();
                for (auto& received : packets)
                {
                    if (received.type != PacketType::StatusResponse) continue;

                    PacketReader reader(received.payload.data(), received.payload.size());
                    StatusResponsePacket response = deserializeStatusResponse(reader);

                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        state->packet = std::move(response);
                    }
                    state->success = true;
                    state->done = true;
                    client.disconnect();
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }

            client.disconnect();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[ServerPreview] probe failed: " << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "[ServerPreview] probe failed: unknown exception\n";
        }
        state->done = true;
    });
}

void MenuGameState::handleEvent(const sf::Event& event)
{
    worldList.handleEvent(event);
    quit.handleEvent(event);
    player.handleEvent(event);
    nicknameField.handleEvent(event);
}

void MenuGameState::update(float dt)
{
    auto size = game->getWindow().getSize();

    quit.updateScreenRelative(size);
    worldList.updateScreenRelative(size);

    worldList.position = {size.x * 0.75f, 0.0f};
    worldList.size = {size.x * 0.25f, static_cast<float>(size.y)};

    float fieldW = size.y * 0.32f;
    float fieldH = size.y * 0.06f;
    float fieldX = (size.x * 0.75f - fieldW) * 0.5f;
    float fieldY = size.y * 0.78f;
    nicknameField.position = {fieldX, fieldY};
    nicknameField.size = {fieldW, fieldH};

    worldList.setNickname(nicknameField.getText());

    worldList.update(dt);
    quit.update(dt);

    player.update(dt);
    nicknameField.update(dt);

    if (activeProbe && activeProbe->done)
    {
        if (activeProbe->success)
        {
            std::lock_guard<std::mutex> lock(activeProbe->mutex);
            statusPacket = activeProbe->packet;
            hasStatus = true;
        }
        activeProbe.reset();
        if (probeThread.joinable()) probeThread.detach();
    }

    std::string currentIp = worldList.getIpText();

    if (currentIp != lastIp)
    {
        lastIp = currentIp;
        ipDebounce = 0.0f;
        debouncePending = true;
        hasStatus = false;
        cancelActiveProbe();
    }
    else if (debouncePending)
    {
        ipDebounce += dt;
        if (ipDebounce >= IP_DEBOUNCE_SECONDS)
        {
            debouncePending = false;
            if (!currentIp.empty()) beginProbe(currentIp);
        }
    }

    sf::FloatRect preview_rect = worldList.serverPreviewArea();
    serverPreview.position = preview_rect.position;
    serverPreview.size = preview_rect.size;
    serverPreview.update(dt);
}

void MenuGameState::render(sf::RenderWindow& window)
{
    window.setView(sf::View(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)})));

    sf::Sprite background(backgroundTexture);
    background.setPosition({0.0f, 0.0f});
    background.setScale
    (
        {
            static_cast<float>(window.getSize().x) / background.getTexture().getSize().x,
            static_cast<float>(window.getSize().y) / background.getTexture().getSize().y
        }
    );
    window.draw(background);

    sf::RectangleShape logo;
    logo.setTexture(&blockbit_logo);

    logo.setSize({static_cast<float>(logo.getTexture()->getSize().x), static_cast<float>(logo.getTexture()->getSize().y)});

    float factor = (static_cast<float>(window.getSize().x) / 5.0f) / static_cast<float>(logo.getTexture()->getSize().x);
    logo.setScale({factor, factor});

    logo.setPosition({50.0f, 50.0f});

    window.draw(logo);


    worldList.render(window);

    if (hasStatus && worldList.multiplayerActive()) serverPreview.render(window);
    quit.render(window);
    player.render(window);

    {
        sf::Vector2f fpos = nicknameField.position;
        sf::Vector2f fsize = nicknameField.size;

        sf::Text label(AssetManager::getFont(AssetManager::FontID::PressStart2P), "Nickname", static_cast<unsigned>(fsize.y * 0.4f));
        label.setFillColor(sf::Color(230, 230, 230));
        label.setOutlineColor(sf::Color::Black);
        label.setOutlineThickness(1.0f);
        label.setPosition({fpos.x, fpos.y - fsize.y * 0.55f});
        window.draw(label);
    }
    nicknameField.render(window);
}

void MenuGameState::onObscured()
{
    GameState::onObscured();

    game->jukebox.reset();
}