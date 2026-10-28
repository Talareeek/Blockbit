#include "../include/MenuGameState.hpp"
#include "../include/Game.hpp"
#include "../include/AssetManager.hpp"
#include "../include/Client.hpp"
#include "../include/Render.hpp"
#include "../include/Settings.hpp"

#include <imgui.h>

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
    if(!blockbit_logo.loadFromFile("resources/textures/blockbit_logo.png")) throw std::runtime_error("Failed loading resources/textures/blockbit_logo.png");


    UIElement::ScreenRelative settingsRelative{{0.015f, 0.82f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};
    UIElement::ScreenRelative quitRelative    {{0.015f, 0.87f}, {0.17f, 0.035f}, UIElement::ScreenRelative::ScaleMode::Stretch};

    std::filesystem::path savesPath = getWorldsPath();
    std::filesystem::create_directories(savesPath);
    worldList = WorldList(savesPath, game);

    quit = TextButton(quitRelative, "Quit", [this]()
    {
        this->show_exit_confirmation = true;
    });
    quit.setColors(sf::Color(210, 210, 210), sf::Color(230, 80, 80));
    quit.updateScreenRelative(game->getWindow().getSize());

    settings = TextButton(settingsRelative, "Settings", [this]()
    {
        this->game->getSettingsWindow().show();
    });
    settings.setColors(sf::Color(210, 210, 210), Settings::ui.main_color);
    settings.updateScreenRelative(game->getWindow().getSize());

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

            client.send(serializePlainPacket(PacketType::StatusRequest));

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
    settings.handleEvent(event);
    player.handleEvent(event);
}

void MenuGameState::update(float dt)
{
    auto size = game->getWindow().getSize();

    const float sidebarWidth = size.x * 0.20f;
    const float logoWidth = sidebarWidth * 0.80f;
    const float logoAspect = static_cast<float>(blockbit_logo.getSize().y) / static_cast<float>(blockbit_logo.getSize().x);
    const float logoHeight = logoWidth * logoAspect;
    const float logoBottom = size.y * 0.03f + logoHeight;

    accountSlotPosition = {sidebarWidth * 0.10f, logoBottom + size.y * 0.03f};
    accountSlotSize = {sidebarWidth * 0.80f, size.y * 0.06f};

    quit.updateScreenRelative(size);
    settings.updateScreenRelative(size);
    settings.setColors(sf::Color(210, 210, 210), Settings::ui.main_color);
    worldList.updateScreenRelative(size);

    worldList.position = {size.x * 0.75f, 0.0f};
    worldList.size = {size.x * 0.25f, static_cast<float>(size.y)};

    worldList.update(dt);
    quit.update(dt);
    settings.update(dt);

    player.update(dt);

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

    const float sidebarWidth = window.getSize().x * 0.20f;
    const float windowHeight = static_cast<float>(window.getSize().y);

    sf::RectangleShape sidebar({sidebarWidth, windowHeight});
    sidebar.setPosition({0.0f, 0.0f});
    const sf::Color main = Settings::ui.main_color;
    sidebar.setFillColor(sf::Color(10, 12, 18, 200));
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

    worldList.render(window);

    if (hasStatus && worldList.multiplayerActive()) serverPreview.render(window);
    settings.render(window);
    quit.render(window);
    player.render(window);

    account.performImGui(accountSlotPosition, accountSlotSize);

    renderExitConfirmation();
}

void MenuGameState::renderExitConfirmation()
{
    if(!show_exit_confirmation) return;

    const sf::Vector2u window_size = game->getWindow().getSize();
    const ImVec2 size(360.0f, 140.0f);
    const ImVec2 position(
        (static_cast<float>(window_size.x) - size.x) * 0.5f,
        (static_cast<float>(window_size.y) - size.y) * 0.5f
    );

    ImGui::SetNextWindowPos(position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoSavedSettings;

    bool open = true;
    if(ImGui::Begin("Quit", &open, flags))
    {
        const char* text = "Do you want to quit?";
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        ImGui::SetCursorPosX((size.x - text_size.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
        ImGui::TextUnformatted(text);

        ImGui::Dummy(ImVec2(0.0f, 12.0f));

        const float button_width = 110.0f;
        const float spacing = 20.0f;
        ImGui::SetCursorPosX((size.x - button_width * 2.0f - spacing) * 0.5f);

        if(ImGui::Button("Yes", ImVec2(button_width, 0.0f)))
        {
            show_exit_confirmation = false;
            game->popState(this);
        }
        ImGui::SameLine(0.0f, spacing);
        if(ImGui::Button("No", ImVec2(button_width, 0.0f)))
        {
            show_exit_confirmation = false;
        }
    }
    ImGui::End();

    if(!open) show_exit_confirmation = false;
}

void MenuGameState::onObscured()
{
    GameState::onObscured();

    game->jukebox.reset();
}

void MenuGameState::onRevealed()
{
    GameState::onRevealed();

    worldList.refresh();
}