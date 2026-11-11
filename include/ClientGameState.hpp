#ifndef CLIENT_GAME_STATE_HPP
#define CLIENT_GAME_STATE_HPP

#include "GameState.hpp"
#include "World.hpp"
#include "ClientTransport.hpp"
#include "GameServer.hpp"
#include "Packet.hpp"
#include "PlayerIdentity.hpp"
#include "HealthBar.hpp"
#include "Hotbar.hpp"
#include "Input.hpp"
#include "Chat.hpp"
#include "ChatUI.hpp"
#include "RCon.hpp"
#include "ServerAdminPanel.hpp"
#include "TouchInputOverlay.hpp"
#include "GamepadInput.hpp"
#include "View.hpp"
#include "ParticleManager.hpp"

#include <memory>
#include "ClientWorld.hpp"
#include <optional>
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

class ClientGameState : public GameState
{
private:

    ClientWorld local_world;

    std::unique_ptr<ClientTransport> transport;

    std::optional<GameServer> local_server;

    RCon rcon;

    // Only constructed when local_server is present (listen/singleplayer).
    // Guarantees the admin panel never runs against a server living in a
    // different PID — same-PID is the current access policy.
    std::unique_ptr<ServerAdminPanel> admin_panel;

    uint16_t tick_rate = 60;

    uint64_t latest_tick = 0;
    // Monotonic wall-clock render clock, in server-tick units. Advances by
    // dt/tick_step each frame so interpolation reads fractional sample
    // positions (alpha != 0) instead of snapping to integer ticks every
    // snapshot. Re-anchored lazily on the first snapshot and clamped each
    // frame so it stays INTERPOLATION_DELAY_TICKS behind latest_tick.
    double render_tick_clock = 0.0;
    bool   render_tick_anchored = false;
    // Fractional ticks that remote entities are rendered behind latest_tick.
    // At 120 Hz this is ~67 ms — big enough to absorb typical residential
    // jitter (~30 ms) and the occasional retransmit without forcing the
    // interpolator into the dead-reckoning branch.
    static constexpr double INTERPOLATION_DELAY_TICKS = 8.0;
    // Soft cap on how far behind latest_tick the render clock is allowed
    // to drift before being nudged forward (e.g. after a long network stall).
    // Expressed in ticks — at 120 Hz, 24 ticks is 200 ms.
    static constexpr double RENDER_CLOCK_MAX_LAG = 24.0;
    // Maximum wall-clock dt (seconds) that may advance the render clock in
    // one frame. Caps spikes from alt-tab/stall/GC so render_tick_clock
    // doesn't lurch forward and force a teleport correction.
    static constexpr float MAX_FRAME_DT = 1.0f / 15.0f;

    std::optional<UUID> local_player_entity_id;
    UUID my_entity_id;

    bool initialized = false;
    bool connection_failed = false;
    bool was_connected = false;
    bool connect_attempted = false;
    bool server_unrecoverable = false;

    std::string error_message;
    std::string remote_address;
    std::string pending_host;
    uint16_t pending_port = 0;
    PlayerIdentity identity;
    bool login_sent = false;
    
    std::unique_ptr<ClientView> content_view;

    bool player_ui_initialized = false;

    bool debug = false;
    bool hide_ui = false;

    bool free_camera = false;
    sf::Vector2<double> free_camera_position{};
    static constexpr double FREE_CAMERA_SPEED = 20.0;

    int fps = 0;
    float last_fps_update = 1.0f;

    float since_last_tick = 0.0f;

    std::vector<Input> inputs;

    // Monotonic sequence number for outgoing InputPackets. 0 is reserved as
    // "no input yet", so counting starts at 1. Paired with
    // pending_inputs/last_input_seq_applied so applySnapshot can drop
    // acknowledged batches and replay the rest on top of the authoritative
    // snapshot — this is the whole point of input reconciliation.
    uint64_t next_input_seq = 1;

    struct PendingInputBatch
    {
        uint64_t seq;
        std::vector<Input> inputs;
    };
    // Unacknowledged input batches, oldest-first. Each entry is a copy of
    // what we sent in that tick's InputPacket. applySnapshot drops entries
    // with seq <= snapshot.last_input_seq and replays what's left.
    std::deque<PendingInputBatch> pending_inputs;

    // Highest seq the server has told us it applied. Mirrors
    // ClientData::last_input_seq_applied on the server. Non-decreasing.
    uint64_t last_input_seq_applied = 0;

    // Replays `pending_inputs` on top of the current (freshly applied) state
    // of the local player entity. Called at the end of applySnapshot once
    // the authoritative TransformComponent/PhysicsComponent have been
    // deserialized onto the local player.
    void reconcileLocalPlayer();

    bool pending_screenshot = false;

    Chat chat;
    ChatUI chat_ui;

    float chat_close_cooldown = 0.0f;
    float view_close_cooldown = 0.0f;

    float furnace_animation_timer = 0.0f;

    sf::Vector2<double> camera;
    float view_scale = 1.0f;

    // Pure visual effects layer. Not an entity, not snapshotted — driven by
    // server bursts (explosions, damage) and client-only events (block break,
    // ambient torch/furnace/lava).
    ParticleManager particles;

    // Rolling sample of HealthComponent.since_last_damaged per entity so the
    // client can spot a fresh damage event when the server resets the field.
    std::unordered_map<UUID, float> prev_damage_flash;

    // Ambient-particle scan runs on a slow cadence; hitting every frame would
    // be wasteful and would pile up flames/smoke faster than they fade.
    float ambient_particle_timer = 0.0f;
    static constexpr float AMBIENT_PARTICLE_INTERVAL = 0.1f;

    // Smoothed precipitation intensities, driven toward 0/1 based on the
    // authoritative WeatherState in local_world.weather. The fade is purely a
    // client-side feel thing — the server flips the enum instantly, the
    // renderer eases it over WEATHER_FADE_SECONDS so rain doesn't pop in.
    float rain_intensity = 0.0f;
    float thunder_intensity = 0.0f;
    static constexpr float WEATHER_FADE_SECONDS = 4.0f;

    // Decays over LIGHTNING_FLASH_DURATION whenever a new LightningBolt entity
    // is seen in an incoming snapshot. Drives renderLightningFlash. Not
    // snapshotted — purely a client-side reaction to the first time we see
    // each strike.
    float lightning_flash = 0.0f;
    static constexpr float LIGHTNING_FLASH_DURATION = 0.4f;

    void updateWeatherIntensity(float dt);

    // Horizontal distance (in blocks) walked by the local player since the
    // last footstep puff. We emit a burst every FOOTSTEP_STRIDE blocks so
    // the cadence scales with speed, not frame time.
    double footstep_distance = 0.0;
    std::optional<double> footstep_last_x;
    static constexpr double FOOTSTEP_STRIDE = 0.8;

    // Virtual on-screen controls. Alive on every platform but only receives
    // sf::Event::TouchXxx events on Android; renders nothing on desktop.
    TouchInputOverlay touch_overlay;

    // Gamepad polling + cursor renderer. Appends to the same inputs vector
    // the keyboard path feeds; its cursor overrides the mouse cursor in the
    // outgoing ClientSnapshotPacket while the right stick is in use.
    GamepadInput gamepad;

    void updateViewTransform();

    void tryInitializePlayerUI();
    bool hasPlayerEntity() const;

    void processIncoming();
    void sendTickInputs();
    void applySnapshot(const SnapshotPacket& snapshot);
    void saveScreenshot(sf::RenderWindow& window);

    void updateAmbientParticles(float dt);
    void updateFootstepParticles(float dt);
    void observeDamageParticles();

    void onTick(float tick_step);

    bool acceptsPlayerInput() const { return !chat_ui.isActive() && chat_close_cooldown <= 0.0f; }

    std::string debugString();

public:

    bool isLocalSession() const { return local_server.has_value(); }

    void saveLocalWorld() { if (local_server) local_server->saveWorld(); }

    ClientGameState(Game* game, std::filesystem::path world_path, uint16_t network_port, std::string nickname);

    ClientGameState(Game* game, std::string name, unsigned int seed, GenerationProperties generation_properties, uint16_t network_port, std::string nickname);

    ClientGameState(Game* game, const std::string& host, uint16_t port, std::string nickname);

    ~ClientGameState() override;

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void render(sf::RenderWindow& window) override;

    bool alwaysUpdated() const override;

    static constexpr uint16_t DEFAULT_PORT = GameServer::DEFAULT_PORT;
};

#endif // CLIENT_GAME_STATE_HPP
