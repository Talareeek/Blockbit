cmake_minimum_required(VERSION 3.24)
project(Blockbit CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Compiler flags for debugging
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Debug information: -g3 for full debug info
    # Optimization: -O0 for accurate debugging
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
endif()

# Find SFML
find_package(SFML REQUIRED COMPONENTS Graphics Window System Audio)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(Blockbit ${SOURCES})

# Include directories
target_include_directories(Blockbit PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link libraries
target_link_libraries(Blockbit PRIVATE 
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
)

# Enable debug info in Release mode as well
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(Blockbit PRIVATE -g)
    endif()
endif()

# Print build info
message(STATUS "========================================")
message(STATUS "Blockbit Build Configuration")
message(STATUS "========================================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "SFML Libraries: ${SFML_LIBRARIES}")
message(STATUS "========================================")
