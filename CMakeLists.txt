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

# Find SFML
find_package(SFML REQUIRED COMPONENTS Graphics Window System Audio)

# Threads (asio)
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

# Dear ImGui + ImGui-SFML (fetched at configure time, no submodules to init)
include(FetchContent)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.91.9b
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(IMGUI_SFML_FIND_SFML OFF)   # SFML already found above
set(IMGUI_SFML_IMGUI_DEMO ON)   # ImGui::ShowDemoWindow()
FetchContent_Declare(imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml
    GIT_TAG v3.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(imgui-sfml)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(Blockbit ${SOURCES})

# Include directories
target_include_directories(Blockbit PRIVATE ${CMAKE_SOURCE_DIR}/include)

# asio standalone (no Boost)
target_compile_definitions(Blockbit PRIVATE ASIO_STANDALONE)

# Link libraries
target_link_libraries(Blockbit PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    Threads::Threads
    tinyxml2::tinyxml2
    ImGui-SFML::ImGui-SFML
    ${ZSTD_TARGET}
)

# Enable debug info in Release mode as well
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(Blockbit PRIVATE -g)
    endif()
endif()