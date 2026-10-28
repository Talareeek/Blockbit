cmake_minimum_required(VERSION 3.28)

# Wrapper CMakeLists invoked by Gradle's externalNativeBuild. All real build
# logic lives at the repo root — this just reroutes into it. The repo-root
# CMakeLists.txt has an `if(ANDROID)` early branch that defines libBlockbit.so.
#
# Path math: this file lives at
#     <repo>/android/app/src/main/cpp/CMakeLists.txt
# so five levels up is the repo root.
add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..
    ${CMAKE_BINARY_DIR}/blockbit
)
