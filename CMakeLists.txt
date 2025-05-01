cmake_minimum_required(VERSION 3.19)

project(hyprquery LANGUAGES CXX VERSION 0.1.0)

# Enable compile commands export for clangd and other tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# If using clangd, run these commands after building:
#   cd /path/to/project
#   ln -sf build/compile_commands.json .

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# Find required packages
find_package(PkgConfig REQUIRED)
# Removing system hyprlang dependency
# pkg_check_modules(HYPRLANG REQUIRED hyprlang)
pkg_check_modules(SPDLOG REQUIRED spdlog)

# Include FetchContent for dependencies
include(FetchContent)

# Fetch and build hyprlang directly
FetchContent_Declare(
    hyprlang
    GIT_REPOSITORY https://github.com/hyprwm/hyprlang
    GIT_TAG v0.2.1  # Adjust version as needed
)
FetchContent_MakeAvailable(hyprlang)

# Find CLI11
find_package(CLI11 QUIET)
if (NOT CLI11_FOUND)
    FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.5.0
    )
    FetchContent_MakeAvailable(CLI11)
endif()

# Find nlohmann_json
find_package(nlohmann_json QUIET)
if (NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Add source files
set(SOURCES
    src/main.cpp
    src/ConfigUtils.cpp
    src/SourceHandler.cpp
    # Removing HyprlangCompat.cpp as we're building hyprlang directly
    # src/HyprlangCompat.cpp
)

# Create executable
add_executable(hyq ${SOURCES})

# Include directories
target_include_directories(hyq PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # Using the hyprlang target directly instead of system paths
    # ${HYPRLANG_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(hyq PRIVATE
    # Replace system hyprlang with the built one
    # ${HYPRLANG_LIBRARIES}
    hyprlang
    ${SPDLOG_LIBRARIES}
    CLI11::CLI11
    nlohmann_json::nlohmann_json
)
