#ifndef MENU_GAME_STATE_HPP
#define MENU_GAME_STATE_HPP

#include "GameState.hpp"
#include "Button.hpp"
#include "AccountWidget.hpp"
#include "Slot.hpp"
#include "WorldList.hpp"
#include "LobbyPlayerElement.hpp"
#include "Slider.hpp"
#include "InputField.hpp"
#include "ServerPreview.hpp"
#include "Packet.hpp"

#include "Hotbar.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class MenuGameState : public GameState
{
private:

    sf::Texture blockbit_logo;

    Button quit;
    WorldList worldList;
    LobbyPlayerElement player;
    InputField nicknameField;

    StatusResponsePacket statusPacket{};
    bool hasStatus = false;
    ServerPreview serverPreview;

    std::string lastIp;
    float ipDebounce = 0.0f;
    bool debouncePending = false;

    struct ProbeState
    {
        std::atomic<bool> cancelled{false};
        std::atomic<bool> done{false};
        std::atomic<bool> success{false};
        std::mutex mutex;
        StatusResponsePacket packet;
    };

    std::shared_ptr<ProbeState> activeProbe;
    std::thread probeThread;

    sf::Texture backgroundTexture;

    void beginProbe(const std::string& text);
    void cancelActiveProbe();

public:

    MenuGameState(Game* game);
    ~MenuGameState();

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;
};

#endif // MENU_GAME_STATE_HPP
