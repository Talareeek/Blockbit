cmake_minimum_required(VERSION 3.24)
project(Blockbit C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Debug information: -g3 for full debug info
    # Optimization: -O0 for accurate debugging
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
endif()

# Warning flags applied only to project-owned targets (bb_core_client,
# bb_core_server, Blockbit, BlockbitServer) — never to fetched dependencies
# such as imgui, imgui-sfml, or nlohmann_json.
set(BLOCKBIT_WARNING_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(BLOCKBIT_WARNING_FLAGS -Wall)
endif()

# Opt-out release flavour for the headless/dedicated server: pure -O3,
# no debug info. Driven by build-headless.sh (sets BLOCKBIT_HEADLESS_STRIP=ON).
option(BLOCKBIT_HEADLESS_STRIP "Build BlockbitServer with release flags stripped of debug info" OFF)

find_package(SFML REQUIRED COMPONENTS Graphics Window System Audio)
find_package(Threads REQUIRED)

# zstd
find_package(zstd CONFIG QUIET)
if(TARGET zstd::libzstd_shared)
    set(ZSTD_TARGET zstd::libzstd_shared)
elseif(TARGET zstd::libzstd_static)
    set(ZSTD_TARGET zstd::libzstd_static)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZSTD QUIET IMPORTED_TARGET libzstd)
    endif()
    if(TARGET PkgConfig::ZSTD)
        set(ZSTD_TARGET PkgConfig::ZSTD)
    else()
        find_path(ZSTD_INCLUDE_DIR zstd.h REQUIRED)
        find_library(ZSTD_LIBRARY NAMES zstd zstd_static REQUIRED)
        add_library(zstd_manual UNKNOWN IMPORTED)
        set_target_properties(zstd_manual PROPERTIES
            IMPORTED_LOCATION "${ZSTD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZSTD_INCLUDE_DIR}"
        )
        set(ZSTD_TARGET zstd_manual)
    endif()
endif()

find_package(tinyxml2 REQUIRED)

include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.9b
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF)
set(IMGUI_SFML_IMGUI_DEMO ON)
FetchContent_Declare(imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_TAG v3.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui-sfml)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)


# -----------------------------------------------------------------------------
# Source partitioning
# -----------------------------------------------------------------------------
# Everything under src/*.cpp is bucketed:
#
#   BLOCKBIT_CLIENT_ONLY_SOURCES
#       Pure UI/menu/window/local-client code. Only linked into Blockbit.
#
#   BLOCKBIT_ENTRY_CLIENT        -> main.cpp (Blockbit entry)
#   BLOCKBIT_ENTRY_HEADLESS      -> HeadlessMain.cpp (BlockbitServer entry)
#
#   BLOCKBIT_CORE_SOURCES
#       Everything else. Compiled into TWO object libraries with different
#       defines:
#           bb_core_client — plain build; ClientX view classes compile normally
#           bb_core_server — HEADLESS_BUILD defined; ClientX bodies and
#                            graphics-only command handlers are excluded
#
#   Why double-compile:
#       The View .cpp files (ChestView/FurnaceView/InventoryView/CraftingView)
#       contain BOTH the server-side ServerXxxView (needed by GameServer) and
#       the ImGui-tied ClientXxxView in the same translation unit. Keeping the
#       GameServer linkable without touching UIElement/Slot/renderItemInfo
#       means those TUs have to be compiled a second time with HEADLESS_BUILD
#       set so the client bodies drop out.
#
#   AssetManager.cpp and Input.cpp are in the shared bucket because:
#       - Block.hpp has an inline lambda that calls AssetManager::getGameTexture
#         (default_render_rect), so Block.cpp needs AssetManager symbols at
#         link time whether or not we're on headless.
#       - processWorldInputs() lives in Input.cpp and is called by GameServer.
file(GLOB_RECURSE _ALL_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(_CLIENT_ONLY_BASENAMES
    Account.cpp
    AccountWidget.cpp
    AnnouncementState.cpp
    AuthorGameState.cpp
    BlockAtlas.cpp
    Button.cpp
    ChatUI.cpp
    Checkbox.cpp
    Client.cpp
    ClientGameState.cpp
    Console.cpp
    CreateWorldState.cpp
    Game.cpp
    GameState.cpp
    GenerationPreview.cpp
    HealthBar.cpp
    Hotbar.cpp
    InputField.cpp
    InputManager.cpp
    IntroGameState.cpp
    Jukebox.cpp
    Label.cpp
    LobbyPlayerElement.cpp
    MenuGameState.cpp
    NetworkClientTransport.cpp
    PauseScreenState.cpp
    RCon.cpp
    Render.cpp
    RenderSystem.cpp
    ServerPreview.cpp
    SettingsWindow.cpp
    Slider.cpp
    Slot.cpp
    UIElement.cpp
    UILayout.cpp
    WorldEntry.cpp
    WorldList.cpp
)

set(_HEADLESS_ONLY_BASENAMES
    HeadlessMain.cpp
)

set(_CLIENT_ENTRY_BASENAMES
    main.cpp
)

set(BLOCKBIT_CORE_SOURCES "")
set(BLOCKBIT_CLIENT_ONLY_SOURCES "")
set(BLOCKBIT_ENTRY_HEADLESS "")
set(BLOCKBIT_ENTRY_CLIENT "")

foreach(_src ${_ALL_SOURCES})
    get_filename_component(_name "${_src}" NAME)
    if(_name IN_LIST _CLIENT_ENTRY_BASENAMES)
        list(APPEND BLOCKBIT_ENTRY_CLIENT "${_src}")
    elseif(_name IN_LIST _HEADLESS_ONLY_BASENAMES)
        list(APPEND BLOCKBIT_ENTRY_HEADLESS "${_src}")
    elseif(_name IN_LIST _CLIENT_ONLY_BASENAMES)
        list(APPEND BLOCKBIT_CLIENT_ONLY_SOURCES "${_src}")
    else()
        list(APPEND BLOCKBIT_CORE_SOURCES "${_src}")
    endif()
endforeach()

# Interface target that factors out the common include dirs, defines, and
# transitive link deps the two bb_core variants share. Avoids repeating the
# find_package targets on both OBJECT libraries.
add_library(bb_core_deps INTERFACE)
target_include_directories(bb_core_deps INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(bb_core_deps INTERFACE ASIO_STANDALONE)
target_link_libraries(bb_core_deps INTERFACE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    Threads::Threads
    tinyxml2::tinyxml2
    nlohmann_json::nlohmann_json
    ${ZSTD_TARGET}
)

# -----------------------------------------------------------------------------
# bb_core_client: shared core compiled WITHOUT HEADLESS_BUILD.
# -----------------------------------------------------------------------------
add_library(bb_core_client OBJECT ${BLOCKBIT_CORE_SOURCES})
target_link_libraries(bb_core_client PUBLIC bb_core_deps)
target_compile_options(bb_core_client PRIVATE ${BLOCKBIT_WARNING_FLAGS})

# -----------------------------------------------------------------------------
# bb_core_server: same sources compiled WITH HEADLESS_BUILD.
# -----------------------------------------------------------------------------
add_library(bb_core_server OBJECT ${BLOCKBIT_CORE_SOURCES})
target_link_libraries(bb_core_server PUBLIC bb_core_deps)
target_compile_definitions(bb_core_server PRIVATE HEADLESS_BUILD)
target_compile_options(bb_core_server PRIVATE ${BLOCKBIT_WARNING_FLAGS})

# -----------------------------------------------------------------------------
# Blockbit: client executable (behavior unchanged).
# -----------------------------------------------------------------------------
add_executable(Blockbit
    ${BLOCKBIT_ENTRY_CLIENT}
    ${BLOCKBIT_CLIENT_ONLY_SOURCES}
    $<TARGET_OBJECTS:bb_core_client>
)

target_link_libraries(Blockbit PRIVATE
    bb_core_deps
    ImGui-SFML::ImGui-SFML
)
target_compile_options(Blockbit PRIVATE ${BLOCKBIT_WARNING_FLAGS})

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(Blockbit PRIVATE -g)
    endif()
endif()

# -----------------------------------------------------------------------------
# BlockbitServer: headless/dedicated server.
# -----------------------------------------------------------------------------
add_executable(BlockbitServer
    ${BLOCKBIT_ENTRY_HEADLESS}
    $<TARGET_OBJECTS:bb_core_server>
)

target_link_libraries(BlockbitServer PRIVATE bb_core_deps)
target_compile_definitions(BlockbitServer PRIVATE HEADLESS_BUILD)
target_compile_options(BlockbitServer PRIVATE ${BLOCKBIT_WARNING_FLAGS})

if(BLOCKBIT_HEADLESS_STRIP AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(BlockbitServer PRIVATE -O3)
    target_link_options(BlockbitServer PRIVATE -s)
endif()
