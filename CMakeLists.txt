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

# Desktop builds always include ValveSoftware/GameNetworkingSockets. Android
# returns from its dedicated build branch below before desktop dependencies.

# Third-party CMake dependencies are fetched from their upstream repositories.
# Threads is provided by CMake; OpenSSL and Protobuf are system dependencies
# required by GameNetworkingSockets when BLOCKBIT_WITH_GNS is enabled.
include(FetchContent)

# SFML — windowing/graphics/audio. Same source on desktop and Android.
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

# zstd — static lib only. Its CMakeLists lives under build/cmake.
set(ZSTD_BUILD_STATIC   ON  CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED   OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
# Note: github.com/facebook/zstd 404s as of late 2026; upstream now lives
# under the original author's account.
FetchContent_Declare(zstd
    GIT_REPOSITORY https://github.com/Cyan4973/zstd.git
    GIT_TAG v1.5.7
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SOURCE_SUBDIR build/cmake
)

# tinyxml2
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 10.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
)

# asio — standalone, header-only. Blockbit links against this + ASIO_STANDALONE.
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG asio-1-30-2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
)

# imgui
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.9b
    GIT_SHALLOW ON
)

# nlohmann_json
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)

# -----------------------------------------------------------------------------
# Android: minimal scaffold. Phase 3 ships a green-window stub (src/android_main.cpp)
# packaged as libBlockbit.so for a NativeActivity. Later phases will progressively
# enable real Blockbit sources behind __ANDROID__ guards and swap system deps
# (asio/zstd/tinyxml2) for FetchContent variants.
# -----------------------------------------------------------------------------
if(ANDROID)
    # Note: when the Android gradle wrapper does `add_subdirectory(<repo root>)`,
    # CMAKE_SOURCE_DIR points at the wrapper's dir (android/app/src/main/cpp),
    # not here. CMAKE_CURRENT_SOURCE_DIR is the repo root while this file runs.

    FetchContent_MakeAvailable(SFML)
    FetchContent_MakeAvailable(zstd)
    FetchContent_MakeAvailable(tinyxml2)

    FetchContent_MakeAvailable(asio)
    add_library(asio_header_only INTERFACE)
    target_include_directories(asio_header_only INTERFACE ${asio_SOURCE_DIR}/asio/include)

    # imgui / imgui-sfml / nlohmann_json match the desktop configuration below.
    FetchContent_MakeAvailable(imgui)

    # imgui-sfml's internal FindImGui.cmake reads IMGUI_DIR from the cache; a
    # plain set() wouldn't survive the FetchContent subdirectory scope.
    set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
    # The Android NDK toolchain sets CMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY,
    # which restricts find_path to the sysroot. The imgui_SOURCE_DIR lives
    # outside of it, so FindImGui's find_path(IMGUI_INCLUDE_DIR …) would
    # cache a NOTFOUND. Pre-seed the cache entry so find_path short-circuits.
    set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)

    # imgui-sfml does find_package(OpenGL REQUIRED) and links OpenGL::GL.
    # Android ships GLESv2/EGL, not desktop GL. Drop a shim FindOpenGL.cmake
    # into a dir we prepend to CMAKE_MODULE_PATH so imgui-sfml finds our
    # replacement before CMake's bundled one.
    set(_bb_opengl_shim ${CMAKE_BINARY_DIR}/android-opengl-shim)
    file(MAKE_DIRECTORY ${_bb_opengl_shim})
    file(WRITE ${_bb_opengl_shim}/FindOpenGL.cmake
"if(NOT TARGET OpenGL::GL)
    add_library(OpenGL::GL INTERFACE IMPORTED)
    set_target_properties(OpenGL::GL PROPERTIES
        INTERFACE_LINK_LIBRARIES \"GLESv2;EGL\"
    )
endif()
set(OpenGL_FOUND TRUE)
set(OPENGL_FOUND TRUE)
")
    list(PREPEND CMAKE_MODULE_PATH ${_bb_opengl_shim})

    set(IMGUI_SFML_FIND_SFML OFF)
    set(IMGUI_SFML_IMGUI_DEMO OFF)
    # imgui-sfml 3.0's render path uses desktop-GL fixed-function calls
    # (glPolygonMode, glPopAttrib, GL_POLYGON_MODE) that don't exist in
    # GLES. Strip them on Android — they're not needed to render ImGui
    # correctly on GLES, just lose a tiny bit of state-restore fidelity.
    FetchContent_Declare(imgui-sfml
        GIT_REPOSITORY https://github.com/SFML/imgui-sfml
        GIT_TAG v3.0
        GIT_SHALLOW ON
        # CMake treats ';' as a list separator in PATCH_COMMAND args, so drop
        # the trailing semicolon from the sed patterns. The leftover ';' after
        # each replacement forms a valid empty C++ statement.
        PATCH_COMMAND sed -i
            -e "s|glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)|/* GLES skip */|"
            -e "s|glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode)|/* GLES skip */|"
            -e "s|glPopAttrib()|/* GLES skip */|"
            # `[[]0[]]` matches a literal `[0]` without needing backslashes
            # (which CMake's PATCH_COMMAND arg processing strips).
            -e "s|glPolygonMode(GL_FRONT, (GLenum)last_polygon_mode[[]0[]])|/* GLES skip */|"
            -e "s|glPolygonMode(GL_BACK, (GLenum)last_polygon_mode[[]1[]])|/* GLES skip */|"
            imgui-SFML.cpp
    )
    FetchContent_MakeAvailable(imgui-sfml)

    FetchContent_MakeAvailable(nlohmann_json)

    # Pull every src/*.cpp except desktop/headless entry points. android_main.cpp
    # provides the Android entry; its contents are guarded by #ifdef __ANDROID__
    # so it compiles to nothing on desktop.
    file(GLOB_RECURSE _ANDROID_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    list(FILTER _ANDROID_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
    list(FILTER _ANDROID_SOURCES EXCLUDE REGEX ".*/HeadlessMain\\.cpp$")

    add_library(Blockbit SHARED ${_ANDROID_SOURCES})
    target_compile_features(Blockbit PRIVATE cxx_std_20)
    target_include_directories(Blockbit PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${imgui_SOURCE_DIR}
    )
    target_compile_definitions(Blockbit PRIVATE ASIO_STANDALONE)
    target_link_libraries(Blockbit PRIVATE
        SFML::Graphics
        SFML::Audio
        ImGui-SFML::ImGui-SFML
        libzstd_static
        tinyxml2::tinyxml2
        nlohmann_json::nlohmann_json
        asio_header_only
        android
        log
    )
    # ANativeActivity_onCreate lives in SFML::Main; whole-archive keeps it from
    # being stripped out of libBlockbit.so so the NativeActivity loader finds it.
    target_link_libraries(Blockbit PUBLIC
        -Wl,--whole-archive
        SFML::Main
        -Wl,--no-whole-archive
    )
    # Google Play requires all new apps to support 16 KB page sizes.
    target_link_options(Blockbit PRIVATE "-Wl,-z,max-page-size=16384")
    return()
endif()

# -----------------------------------------------------------------------------
# Desktop
# -----------------------------------------------------------------------------
# Avoid Freetype's optional HarfBuzz bridge in cross builds. Its static
# archive otherwise has unresolved HarfBuzz references with SFML's MinGW link
# order; the game does not use text shaping features from this bridge.
if(WIN32)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
endif()
FetchContent_MakeAvailable(SFML)
find_package(Threads REQUIRED)

if(NOT ANDROID)
    # Standalone GameNetworkingSockets — same ISteamNetworkingSockets API that
    # the Steam SDK's SDR transport plugs into later, so callsites don't move.
    # Built from source via FetchContent and linked statically.
    find_package(OpenSSL)
    if(NOT OpenSSL_FOUND)
        message(FATAL_ERROR
            "BLOCKBIT_WITH_GNS=ON requires OpenSSL on the system (it cannot be\n"
            "fetched from source the way the other dependencies here are — see\n"
            "the comment near the top of this file).\n"
            "  Arch:   sudo pacman -S openssl\n"
            "  Debian: sudo apt install libssl-dev\n"
            "Install OpenSSL development files and re-run cmake.")
    endif()

    # GNS expects Protobuf's installed CMake package (including protoc). Its
    # bundled compatibility FindProtobuf module conflicts with targets created
    # by add_subdirectory(), so use the system package here.
    find_package(Protobuf CONFIG QUIET)
    if(NOT Protobuf_FOUND)
        find_package(Protobuf REQUIRED)
    endif()

    # GNS's own CMake honours these via option() — set them before
    # MakeAvailable so they take effect for the sub-build.
    set(BUILD_SHARED_LIB  OFF    CACHE BOOL   "" FORCE)
    set(BUILD_EXAMPLES    OFF    CACHE BOOL   "" FORCE)
    set(BUILD_TESTS       OFF    CACHE BOOL   "" FORCE)
    set(BUILD_TOOLS       OFF    CACHE BOOL   "" FORCE)
    set(USE_CRYPTO        OpenSSL CACHE STRING "" FORCE)
    FetchContent_Declare(GameNetworkingSockets
        GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
        GIT_TAG        v1.6.1
        GIT_SHALLOW    ON
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_MakeAvailable(GameNetworkingSockets)

    # GNS produces either GameNetworkingSockets_s (static, the mode we want)
    # or GameNetworkingSockets (shared). Pick whichever the sub-build actually
    # defined — the default moved between release tags.
    if(TARGET GameNetworkingSockets_s)
        set(GNS_TARGET GameNetworkingSockets_s)
    elseif(TARGET GameNetworkingSockets)
        set(GNS_TARGET GameNetworkingSockets)
    else()
        message(FATAL_ERROR "FetchContent pulled GameNetworkingSockets but no expected target was defined.")
    endif()

    # GNS spells this Windows system library with mixed case. MinGW's linker
    # runs on case-sensitive filesystems, where the import library is lowercase.
    if(WIN32 AND MINGW)
        get_target_property(_gns_links ${GNS_TARGET} INTERFACE_LINK_LIBRARIES)
        if(_gns_links)
            list(TRANSFORM _gns_links REPLACE "^Iphlpapi$" "iphlpapi")
            set_target_properties(${GNS_TARGET} PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_gns_links}")
        endif()
    endif()
endif()

# zstd
FetchContent_MakeAvailable(zstd)
if(TARGET libzstd_static)
    set(ZSTD_TARGET libzstd_static)
elseif(TARGET zstd::libzstd_static)
    set(ZSTD_TARGET zstd::libzstd_static)
else()
    message(FATAL_ERROR "FetchContent pulled zstd but no expected static target was defined.")
endif()

# tinyxml2
FetchContent_MakeAvailable(tinyxml2)

# imgui
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
set(IMGUI_SFML_FIND_SFML OFF)
set(IMGUI_SFML_IMGUI_DEMO ON)
FetchContent_Declare(imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_TAG v3.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui-sfml)

FetchContent_MakeAvailable(asio)
add_library(asio_header_only INTERFACE)
target_include_directories(asio_header_only INTERFACE ${asio_SOURCE_DIR}/asio/include)

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
    NetworkAdminSession.cpp
    NetworkClientTransport.cpp
    GnsClientTransport.cpp
    PauseScreenState.cpp
    RCon.cpp
    Render.cpp
    RenderSystem.cpp
    ServerAdminPanel.cpp
    ServerPreview.cpp
    SettingsWindow.cpp
    Slider.cpp
    Slot.cpp
    StructureEditorGameState.cpp
    TextButton.cpp
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
# imgui_SOURCE_DIR is listed so GameCommon.hpp picks up the SAME imgui.h that
# ImGui-SFML (and thus the Blockbit executable) links against. Without this,
# a system-installed imgui.h with a different ImGuiStyle layout gets found
# first, and ImGui::GetStyle().Colors[...] writes land at wrong offsets at
# runtime — heap-corrupting the adjacent fields.
target_include_directories(bb_core_deps INTERFACE
    ${CMAKE_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
)
target_compile_definitions(bb_core_deps INTERFACE ASIO_STANDALONE)
target_link_libraries(bb_core_deps INTERFACE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    Threads::Threads
    tinyxml2::tinyxml2
    nlohmann_json::nlohmann_json
    asio_header_only
    ${ZSTD_TARGET}
)

if(WIN32)
    target_link_libraries(bb_core_deps INTERFACE
        ws2_32 mswsock
        "$<LINK_GROUP:RESCAN,freetype,harfbuzz>"
    )
endif()

target_compile_definitions(bb_core_deps INTERFACE BLOCKBIT_WITH_GNS=1)
target_link_libraries(bb_core_deps INTERFACE ${GNS_TARGET})

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

# -----------------------------------------------------------------------------
# structure_gen: one-off utility that rewrites the mineshaft structure files
# and emits sibling corridor pieces + a jigsaw pool JSON. Not wired to the
# default `all` target; invoke explicitly:
#   ninja -C build structure_gen && build/bin/structure_gen
# Reuses the headless core so it can read/write .bbt files without pulling
# in SFML graphics.
# -----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/structure_gen/main.cpp")
    add_executable(structure_gen EXCLUDE_FROM_ALL
        tools/structure_gen/main.cpp
        $<TARGET_OBJECTS:bb_core_server>
    )
    target_link_libraries(structure_gen PRIVATE bb_core_deps)
    target_compile_definitions(structure_gen PRIVATE HEADLESS_BUILD)
endif()
