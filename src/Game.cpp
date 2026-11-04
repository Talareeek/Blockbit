#include <thread>
#include <chrono>
#include <map>
#include <iostream>
#include <optional>

#include "../include/Game.hpp"
#include "../include/Entity.hpp"
#include "../include/PhysicsSystem.hpp"
#include "../include/RenderSystem.hpp"
#include "../include/PhysicsComponent.hpp"
#include "../include/RenderComponent.hpp"
#include "../include/AssetManager.hpp"
#include "../include/World.hpp"
#include "../include/BlockAtlas.hpp"
#include "../include/AuthorGameState.hpp"
#include "../include/Prefab.hpp"
#include "../include/Settings.hpp"
#include "../include/GameCommon.hpp"

#include <imgui.h>
#include <imgui-SFML.h>

#ifdef __ANDROID__
#include <SFML/System/NativeActivity.hpp>
#include <SFML/System/FileInputStream.hpp>
#include <android/native_activity.h>
#include <jni.h>

// DisplayMetrics.density from the active NativeActivity. Returns 1.0 (desktop-
// equivalent) on any JNI failure so the UI stays functional even if we can't
// read the system value. Cached after the first successful probe — the display
// geometry doesn't change for the lifetime of the process.
static float androidDisplayDensity()
{
    static float cached = -1.0f;
    if (cached > 0.0f) return cached;

    ANativeActivity* activity = sf::getNativeActivity();
    if (!activity) return 1.0f;

    JavaVM* vm = activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED)
    {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return 1.0f;
        attached = true;
    }
    else if (status != JNI_OK)
    {
        return 1.0f;
    }

    float density = 1.0f;
    auto cleanup = [&]() {
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (attached) vm->DetachCurrentThread();
    };

    // activity.getResources().getDisplayMetrics().density
    jclass activityClass = env->GetObjectClass(activity->clazz);
    jmethodID getResources = env->GetMethodID(
        activityClass, "getResources", "()Landroid/content/res/Resources;");
    jobject resources = env->CallObjectMethod(activity->clazz, getResources);
    env->DeleteLocalRef(activityClass);
    if (!resources) { cleanup(); return 1.0f; }

    jclass resourcesClass = env->GetObjectClass(resources);
    jmethodID getDisplayMetrics = env->GetMethodID(
        resourcesClass, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
    jobject metrics = env->CallObjectMethod(resources, getDisplayMetrics);
    env->DeleteLocalRef(resourcesClass);
    env->DeleteLocalRef(resources);
    if (!metrics) { cleanup(); return 1.0f; }

    jclass metricsClass = env->GetObjectClass(metrics);
    jfieldID densityField = env->GetFieldID(metricsClass, "density", "F");
    density = env->GetFloatField(metrics, densityField);
    env->DeleteLocalRef(metricsClass);
    env->DeleteLocalRef(metrics);

    cleanup();

    if (density <= 0.0f) density = 1.0f;
    cached = density;
    return density;
}
#endif

static void installImGuiGlobalFont()
{
    ImGuiIO& io = ImGui::GetIO();
    const char* path = "resources/fonts/PressStart2P-Regular.ttf";

#ifdef __ANDROID__
    // ImGui's AddFontFromFileTTF uses stdio fopen(), which can't see assets
    // packaged inside the APK. Pull the bytes through sf::FileInputStream
    // (which routes to AAssetManager on Android) and hand them to ImGui via
    // AddFontFromMemoryTTF. Scale by display density so the glyphs stay
    // legible on high-DPI phone screens. ImGui-SFML's Init already added the
    // default font, so clear the atlas first to avoid stacking.
    io.Fonts->Clear();

    sf::FileInputStream stream;
    std::optional<std::size_t> size_opt;
    if(!stream.open(path) || !(size_opt = stream.getSize()) || *size_opt == 0)
    {
        std::cerr << "Game: failed to open global ImGui font from assets: " << path << '\n';
        ImFontConfig cfg;
        cfg.SizePixels = 13.0f * androidDisplayDensity();
        io.Fonts->AddFontDefault(&cfg);
        if(!ImGui::SFML::UpdateFontTexture())
            std::cerr << "Game: failed to update ImGui font texture\n";
        return;
    }

    const std::size_t size = *size_opt;
    void* buffer = IM_ALLOC(size);
    auto read_opt = stream.read(buffer, size);
    if(!read_opt || *read_opt != size)
    {
        std::cerr << "Game: failed to read global ImGui font from assets: " << path << '\n';
        IM_FREE(buffer);
        ImFontConfig cfg;
        cfg.SizePixels = 13.0f * androidDisplayDensity();
        io.Fonts->AddFontDefault(&cfg);
        if(!ImGui::SFML::UpdateFontTexture())
            std::cerr << "Game: failed to update ImGui font texture\n";
        return;
    }

    ImFont* font = io.Fonts->AddFontFromMemoryTTF(
        buffer, static_cast<int>(size), 13.0f * androidDisplayDensity());
    if(!font)
    {
        std::cerr << "Game: failed to load global ImGui font\n";
        return;
    }
    io.FontDefault = font;
    if(!ImGui::SFML::UpdateFontTexture())
        std::cerr << "Game: failed to update ImGui font texture\n";
#else
    ImFont* font = io.Fonts->AddFontFromFileTTF(path, 13.0f);
    if(!font)
    {
        std::cerr << "Game: failed to load global ImGui font\n";
        return;
    }
    io.FontDefault = font;
    if(!ImGui::SFML::UpdateFontTexture())
        std::cerr << "Game: failed to update ImGui font texture\n";
#endif
}

Game::Game()
{
    Settings::load();

    fullscreen = Settings::graphics.fullscreen;

#ifdef __ANDROID__
    // On Android the OS owns the display geometry — ignore the saved
    // Settings::graphics.{window_width, height, fullscreen} and always open
    // native-resolution fullscreen.
    window.create(
        sf::VideoMode::getDesktopMode(),
        "Blockbit",
        sf::State::Fullscreen
    );
#else
    window.create(
        fullscreen ? sf::VideoMode::getDesktopMode()
                   : sf::VideoMode({Settings::graphics.window_width, Settings::graphics.window_height}),
        "Blockbit",
        fullscreen ? sf::State::Fullscreen : sf::State::Windowed
    );
#endif
    window.setVerticalSyncEnabled(Settings::graphics.vsync);
    if(Settings::graphics.fps_limit > 0) window.setFramerateLimit(Settings::graphics.fps_limit);

	if (!icon.loadFromFile("resources/textures/grass.png")) throw std::runtime_error("Failed loading an icon");

	window.setIcon(icon);

    if (!ImGui::SFML::Init(window)) throw std::runtime_error("Failed initializing ImGui-SFML");
    installImGuiGlobalFont();

    unit_size = window.getSize().y / 9;

    AssetManager::loadFont(AssetManager::FontID::PressStart2P, "resources/fonts/PressStart2P-Regular.ttf");
    AssetManager::loadFont(AssetManager::FontID::FiraCode, "resources/fonts/FiraCode-Regular.ttf");

    //UI TEXTURES
    AssetManager::loadUITexture(AssetManager::UITextureID::Coin, "resources/textures/coin.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Block_Overlay, "resources/textures/overlay.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Inventory, "resources/textures/inventory.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Hotbar, "resources/textures/hotbar.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::UIBackground, "resources/textures/ui_background.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Logo, "resources/textures/logo.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Chest, "resources/textures/chest_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Furnace, "resources/textures/furnace_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Crafting, "resources/textures/crafting_view.png");
    AssetManager::loadUITexture(AssetManager::UITextureID::Border, "resources/textures/border.png");
    AssetManager::getUITexture(AssetManager::UITextureID::Border).setRepeated(true);


    //GAME TEXTURES
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Player, "resources/textures/player.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dynamite, "resources/textures/dynamite.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bucket, "resources/textures/bucket.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Water_Bucket, "resources/textures/water_bucket.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Pig, "resources/textures/pig.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Lighter, "resources/textures/lighter.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Pickaxe, "resources/textures/wooden_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Pickaxe, "resources/textures/stone_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Pickaxe, "resources/textures/gold_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Pickaxe, "resources/textures/iron_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Pickaxe, "resources/textures/diamond_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Axe, "resources/textures/wooden_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Axe, "resources/textures/stone_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Axe, "resources/textures/gold_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Axe, "resources/textures/iron_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Axe, "resources/textures/diamond_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Wooden_Shovel, "resources/textures/wooden_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone_Shovel, "resources/textures/stone_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Shovel, "resources/textures/gold_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Shovel, "resources/textures/iron_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Shovel, "resources/textures/diamond_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Pickaxe, "resources/textures/ruby_pickaxe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Axe, "resources/textures/ruby_axe.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Shovel, "resources/textures/ruby_shovel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby, "resources/textures/ruby.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stick, "resources/textures/stick.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal, "resources/textures/coal.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Raw_Iron, "resources/textures/raw_iron.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Raw_Gold, "resources/textures/raw_gold.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Ingot, "resources/textures/iron_ingot.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Ingot, "resources/textures/gold_ingot.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond, "resources/textures/diamond.png");

    AssetManager::loadGameTexture(AssetManager::GameTextureID::Stone, "resources/textures/stone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Grass, "resources/textures/grass.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dirt, "resources/textures/dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Cobblestone, "resources/textures/cobblestone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Obsidian, "resources/textures/obsidian.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bedrock, "resources/textures/bedrock.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Water, "resources/textures/water.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Lava, "resources/textures/lava.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal_Ore, "resources/textures/coal_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Ore, "resources/textures/iron_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Ore, "resources/textures/gold_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diamond_Ore, "resources/textures/diamond_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Iron_Block, "resources/textures/iron_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gold_Block, "resources/textures/gold_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Block, "resources/textures/ruby_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coal_Block, "resources/textures/coal_block.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Log, "resources/textures/oak_log.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Leaves, "resources/textures/oak_leaves.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Oak_Planks, "resources/textures/oak_planks.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Birch_Log, "resources/textures/birch_log.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Birch_Leaves, "resources/textures/birch_leaves.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Spruce_Log, "resources/textures/spruce_log.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Spruce_Leaves, "resources/textures/spruce_leaves.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Woodcutter, "resources/textures/woodcutter.png");    
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Fire, "resources/textures/fire.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Sand, "resources/textures/sand.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Coarse_Dirt, "resources/textures/coarse_dirt.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Snow, "resources/textures/snow.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Ore, "resources/textures/ruby_ore.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Chest, "resources/textures/chest.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Bricks, "resources/textures/bricks.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Furnace, "resources/textures/furnace.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Workbench, "resources/textures/workbench.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Gravel, "resources/textures/gravel.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Cactus, "resources/textures/cactus.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Dead_Bush, "resources/textures/dead_bush.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Flint, "resources/textures/flint.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Smooth_Stone, "resources/textures/smooth_stone.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Diorite, "resources/textures/diorite.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Granite, "resources/textures/granite.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Glass, "resources/textures/glass.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Output_Bulb, "resources/textures/output_bulb.png");
    AssetManager::loadGameTexture(AssetManager::GameTextureID::Ruby_Wire, "resources/textures/ruby_wire.png");

    BlockAtlas::add(AssetManager::GameTextureID::Stone);
    BlockAtlas::add(AssetManager::GameTextureID::Grass);
    BlockAtlas::add(AssetManager::GameTextureID::Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Cobblestone);
    BlockAtlas::add(AssetManager::GameTextureID::Obsidian);
    BlockAtlas::add(AssetManager::GameTextureID::Bedrock);
    BlockAtlas::add(AssetManager::GameTextureID::Water);
    BlockAtlas::add(AssetManager::GameTextureID::Lava);
    BlockAtlas::add(AssetManager::GameTextureID::Coal_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Iron_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Gold_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Diamond_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Iron_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Gold_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Coal_Block);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Log);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Leaves);
    BlockAtlas::add(AssetManager::GameTextureID::Oak_Planks);
    BlockAtlas::add(AssetManager::GameTextureID::Birch_Log);
    BlockAtlas::add(AssetManager::GameTextureID::Birch_Leaves);
    BlockAtlas::add(AssetManager::GameTextureID::Spruce_Log);
    BlockAtlas::add(AssetManager::GameTextureID::Spruce_Leaves);
    BlockAtlas::add(AssetManager::GameTextureID::Woodcutter);
    BlockAtlas::add(AssetManager::GameTextureID::Fire);
    BlockAtlas::add(AssetManager::GameTextureID::Sand);
    BlockAtlas::add(AssetManager::GameTextureID::Coarse_Dirt);
    BlockAtlas::add(AssetManager::GameTextureID::Snow);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Ore);
    BlockAtlas::add(AssetManager::GameTextureID::Chest);
    BlockAtlas::add(AssetManager::GameTextureID::Bricks);
    BlockAtlas::add(AssetManager::GameTextureID::Furnace);
    BlockAtlas::add(AssetManager::GameTextureID::Workbench);
    BlockAtlas::add(AssetManager::GameTextureID::Gravel);
    BlockAtlas::add(AssetManager::GameTextureID::Cactus);
    BlockAtlas::add(AssetManager::GameTextureID::Dead_Bush);
    BlockAtlas::add(AssetManager::GameTextureID::Smooth_Stone);
    BlockAtlas::add(AssetManager::GameTextureID::Diorite);
    BlockAtlas::add(AssetManager::GameTextureID::Granite);
    BlockAtlas::add(AssetManager::GameTextureID::Glass);
    BlockAtlas::add(AssetManager::GameTextureID::Output_Bulb);
    BlockAtlas::add(AssetManager::GameTextureID::Ruby_Wire);
    BlockAtlas::build();

    AssetManager::loadShader(AssetManager::ShaderID::Sun, "resources/shaders/sun.frag");
    AssetManager::loadShader(AssetManager::ShaderID::Brightness, "resources/shaders/brightness.frag");
    AssetManager::loadShader(AssetManager::ShaderID::BlockAnimation, "resources/shaders/block_animation.frag");
    AssetManager::loadShader(AssetManager::ShaderID::LightingOverlay, "resources/shaders/lighting_overlay.frag");

    AssetManager::loadSound(AssetManager::SoundID::Click, "resources/sounds/click.wav");

    AssetManager::loadMusic(AssetManager::MusicID::Dream, "resources/sounds/music/dream.mp3");

    PrefabRegistry::init();


    console.assignGame(this);
    console.setupFont("resources/fonts/FiraCode-Regular.ttf");

    settings_window.assignGame(this);    

#ifdef __ANDROID__
    // Scale widget geometry (padding, frame sizes, scrollbar width, …) by the
    // same factor the font was rasterized at. Font pixel size and ScaleAllSizes
    // both grow the UI in window-pixel space; touch coordinates arrive in that
    // same pixel space from SFML, so hit tests stay aligned. (This is why we
    // avoid FontGlobalScale / DisplayFramebufferScale — those would visually
    // upscale without moving hit regions, breaking taps.)
    const float density = androidDisplayDensity();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(density);
#endif
}

void Game::handleEvents()
{
    while(auto event = window.pollEvent())
    {
        processEvent(*event);
    }
}

#ifdef __ANDROID__
// Translate finger 0 touch events into synthesized left-mouse events so that
// ImGui buttons and the custom TextButton/Button widgets — all written for
// the desktop mouse — work unmodified from a tap. The original Touch event
// is still delivered right after, which ClientGameState's TouchInputOverlay
// consumes directly.
static std::optional<sf::Event> touchToMouse(const sf::Event& event)
{
    if (const auto* began = event.getIf<sf::Event::TouchBegan>())
    {
        if (began->finger != 0) return std::nullopt;
        return sf::Event(sf::Event::MouseButtonPressed{
            sf::Mouse::Button::Left, began->position});
    }
    if (const auto* moved = event.getIf<sf::Event::TouchMoved>())
    {
        if (moved->finger != 0) return std::nullopt;
        return sf::Event(sf::Event::MouseMoved{moved->position});
    }
    if (const auto* ended = event.getIf<sf::Event::TouchEnded>())
    {
        if (ended->finger != 0) return std::nullopt;
        return sf::Event(sf::Event::MouseButtonReleased{
            sf::Mouse::Button::Left, ended->position});
    }
    return std::nullopt;
}
#endif

void Game::processEvent(const sf::Event& event)
{
    try
    {
#ifdef __ANDROID__
        // Synthesize a mouse-equivalent event FIRST so the custom widgets
        // (TextButton/Button) react to taps. The real touch event continues
        // through the normal dispatch below for anything that specifically
        // listens for Touch (e.g. ClientGameState's TouchInputOverlay) and for
        // ImGui, which handles touches natively via sf::Touch::isDown().
        //
        // Do NOT feed the synth into ImGui-SFML: its ProcessEvent sets an
        // internal mouseMoved flag on MouseMoved, which then makes Update()
        // overwrite io.MousePos with sf::Mouse::getPosition(window) every
        // frame — a stale/garbage value on Android. That causes ImGui buttons'
        // hover/active state to flash on/off between touch and mouse positions.
        if (const auto synth = touchToMouse(event))
        {
            if (!gameStates.empty()) gameStates.back()->handleEvent(*synth);
            console.handleEvent(*synth);
        }
#endif

        ImGui::SFML::ProcessEvent(window, event);

        if(event.is<sf::Event::Closed>())
        {
            window.close();
        }

        else if(event.is<sf::Event::Resized>())
        {
            sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(event.getIf<sf::Event::Resized>()->size.x), static_cast<float>(event.getIf<sf::Event::Resized>()->size.y)}));
            window.setView(view);
            window.setIcon(icon);
        }

        // SFML sends FocusLost on backgrounding and FocusGained on return.
        // On Android these also fire as MouseLeft / MouseEntered when the
        // activity loses or gains the surface — handle both so the pause
        // path is robust on either platform.

#ifdef __ANDROID__

        else if (event.is<sf::Event::FocusLost>() || event.is<sf::Event::MouseLeft>())
        {
            foreground = false;
        }
        else if (event.is<sf::Event::FocusGained>() || event.is<sf::Event::MouseEntered>())
        {
            foreground = true;
        }

#endif

        else if (event.is<sf::Event::KeyPressed>())
        {
            if(event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::F11)
            {
                Settings::graphics.fullscreen = !Settings::graphics.fullscreen;
                applyGraphicsSettings();
                Settings::save();
            }
            else if(event.getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Grave)
            {
                console_visible = !console_visible;
            }
        }

        if(!gameStates.empty())
        {
            gameStates.back()->handleEvent(event);
        }

        console.handleEvent(event);
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[Game] bad_alloc handling event\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Game] exception handling event: " << e.what() << '\n';
    }
}

#ifdef __ANDROID__
#include <SFML/System/NativeActivity.hpp>
#include <android/log.h>
#include <android/native_activity.h>
#include <jni.h>

namespace
{
    // On NativeActivity apps (hasCode="false") the native surface covers the
    // decorView, so requestFocus() silently no-ops and showSoftInput(view, 0)
    // gets swallowed by IMM. The proven approach — used by imgui-SFML upstream
    // and SDL's native_app_glue — is InputMethodManager.toggleSoftInput with
    // SHOW_FORCED / HIDE_IMPLICIT_ONLY, which doesn't require a focused view.
    void androidSetSoftKeyboard(bool show)
    {
        ANativeActivity* activity = sf::getNativeActivity();
        if (!activity) return;

        JavaVM* vm = activity->vm;
        JNIEnv* env = nullptr;
        bool attached = false;
        jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED)
        {
            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
            attached = true;
        }
        else if (status != JNI_OK)
        {
            return;
        }

        auto cleanup = [&]() {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (attached) vm->DetachCurrentThread();
        };

        jclass activityClass = env->GetObjectClass(activity->clazz);
        jmethodID getSystemService = env->GetMethodID(
            activityClass, "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;");
        jstring serviceName = env->NewStringUTF("input_method");
        jobject imm = env->CallObjectMethod(activity->clazz, getSystemService, serviceName);
        env->DeleteLocalRef(serviceName);
        if (!imm) { env->DeleteLocalRef(activityClass); cleanup(); return; }

        jclass immClass = env->GetObjectClass(imm);
        jmethodID toggleSoftInput = env->GetMethodID(immClass, "toggleSoftInput", "(II)V");

        // Flags: SHOW_FORCED=2 forces the keyboard open regardless of focus;
        // HIDE_IMPLICIT_ONLY=1 dismisses only if the user hasn't explicitly
        // requested it (symmetric partner for SHOW_FORCED).
        constexpr jint SHOW_FORCED = 2;
        constexpr jint HIDE_IMPLICIT_ONLY = 1;
        env->CallVoidMethod(imm, toggleSoftInput, show ? SHOW_FORCED : HIDE_IMPLICIT_ONLY, 0);

        env->DeleteLocalRef(immClass);
        env->DeleteLocalRef(imm);
        env->DeleteLocalRef(activityClass);

        cleanup();
    }
}
#endif

void Game::update()
{
    ImGui::SFML::Update(window, sf::seconds(dt));
    applyImGuiTheme();

#ifdef __ANDROID__
    // Toggle the Android soft keyboard based on ImGui's text-input focus.
    // SFML forwards typed characters back as sf::Event::TextEntered, which
    // ImGui-SFML routes into whatever widget currently owns focus.
    static bool prev_wants_text = false;
    const bool wants_text = ImGui::GetIO().WantTextInput;

    if (wants_text != prev_wants_text)
    {
        androidSetSoftKeyboard(wants_text);
        prev_wants_text = wants_text;
    }
#endif

    handleBufferedStateActions();

    if(gameStates.empty()) return;

    for(auto& state : gameStates)
    {
        if(state == gameStates.back() || state->alwaysUpdated())
        {
            assert(state != nullptr);

            state->update(dt);
        }
    }

    console.update(dt);
}

void Game::render()
{
    window.clear();

    window.resetGLStates();

    if(!gameStates.empty())
    {
        for(auto& state : gameStates)
        {
            if(state->renderWhenObscured() || gameStates.back() == state) state->render(window);
        }
    }
    else
    {
        exit(0);
    }

    console.render();
    settings_window.render();

    ImGui::SFML::Render(window);

    window.display();
}

void Game::run()
{
    // Reset the clock so the first iteration's dt doesn't include the (possibly
    // multi-second) Game constructor time. Without this the first dt is huge
    // and short-timeout states like AuthorGameState's splash skip instantly.
    clock.restart();

    while(window.isOpen())
    {
        if (!foreground)
        {
            // Backgrounded (Android home / app switcher, or desktop focus
            // loss). Block for the next event (waitEvent) so we don't spin,
            // drain any extras, then skip update/render. The next foreground
            // event flips the flag and the normal loop body resumes.
            if (const auto event = window.waitEvent())
                processEvent(*event);
            while (const auto event = window.pollEvent())
                processEvent(*event);
            clock.restart();
            continue;
        }

        dt = clock.restart().asSeconds();

        handleEvents();
        update();
        render();

        if(settings_window.consumeAudioRequest())    applyAudioSettings();
        if(settings_window.consumeGraphicsRequest()) applyGraphicsSettings();
    }

    ImGui::SFML::Shutdown();
}

void Game::pushState(GameState* sender, std::unique_ptr<GameState> state)
{
    if(gameStates.empty() || sender == gameStates.back().get())
    {
        actions_queue.push(GameStateAction{GameStateAction::TYPE::PUSH, std::move(state)});
        console.writeLine(L"Requested push of GameState");
    }
}

void Game::popState(GameState* sender)
{
    if(gameStates.empty() || sender != gameStates.back().get()) return;

    actions_queue.push(GameStateAction{GameStateAction::TYPE::POP, std::nullopt});
    console.writeLine(L"Requested pop of GameState");

}

void Game::handleBufferedStateActions()
{
    while(!actions_queue.empty())
    {
        switch(actions_queue.front().type)
        {
            case GameStateAction::TYPE::PUSH:

                if(!gameStates.empty()) gameStates.back()->onObscured();
                gameStates.emplace_back(std::move(*actions_queue.front().state));
                
                break;

            case GameStateAction::TYPE::POP:

                gameStates.pop_back();
                if(!gameStates.empty()) gameStates.back()->onRevealed();
                break;
        }

        actions_queue.pop();
    }
}

GameState& Game::currentState()
{
    if(gameStates.empty())
    {
        throw std::runtime_error("No game states in stack");
    }
    return *gameStates.back();
}

void Game::popStates(GameState* sender, size_t amount)
{
    for(size_t i = 0; i < amount; i++)
    {
        popState(sender);
    }
}

Account* Game::getAccount() const
{
    return account;
}

sf::RenderWindow& Game::getWindow()
{
    return window;
}

Console& Game::getConsole()
{
    return console;
}

SettingsWindow& Game::getSettingsWindow()
{
    return settings_window;
}

void Game::applyGraphicsSettings()
{
    fullscreen = Settings::graphics.fullscreen;

    window.close();
    ImGui::SFML::Shutdown(window);

    window.create(
        fullscreen ? sf::VideoMode::getDesktopMode()
                   : sf::VideoMode({Settings::graphics.window_width, Settings::graphics.window_height}),
        "Blockbit",
        fullscreen ? sf::State::Fullscreen : sf::State::Windowed
    );

    window.setVerticalSyncEnabled(Settings::graphics.vsync);
    window.setFramerateLimit(Settings::graphics.fps_limit);

    if(!ImGui::SFML::Init(window)) throw std::runtime_error("Failed re-initializing ImGui-SFML");
    installImGuiGlobalFont();

    console.setupFont("resources/fonts/FiraCode-Regular.ttf");

    sf::View view(sf::FloatRect({0.0f, 0.0f}, {static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)}));
    window.setView(view);
    window.setIcon(icon);
}

void Game::applyAudioSettings()
{
    float master = Settings::audio.master_volume / 100.0f;
    jukebox.setMusicVolume(Settings::audio.music_volume * master);
    jukebox.setSoundsVolume(Settings::audio.sound_volume * master);
}