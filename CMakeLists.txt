cmake_minimum_required(VERSION 3.10)

project(hyprquery VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLANG REQUIRED hyprlang)
pkg_check_modules(SPDLOG REQUIRED spdlog)

# Find CLI11
find_package(CLI11 QUIET)
if (NOT CLI11_FOUND)
    include(FetchContent)
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
    include(FetchContent)
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
    src/HyprlangCompat.cpp
)

# Create executable
add_executable(hyq ${SOURCES})

# Include directories
target_include_directories(hyq PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${HYPRLANG_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(hyq PRIVATE
    ${HYPRLANG_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    CLI11::CLI11
    nlohmann_json::nlohmann_json
)
