cmake_minimum_required(VERSION 3.10)
project(hyprquery VERSION 0.1.0)

# Override CMake policy to handle dependencies with older minimum CMake version requirements
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# Tell CMake to try configuring anyway, even for dependencies requiring older CMake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set policy for CMP0048
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()

# Set policy for CMP0127
if(POLICY CMP0127)
  cmake_policy(SET CMP0127 NEW)
endif()

# Suppress developer warnings
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1)

# Add FetchContent module
include(FetchContent)
include(ExternalProject)

# Set the output directory for the binary
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/release)

# Download and configure nlohmann_json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.5
)
FetchContent_MakeAvailable(nlohmann_json)

# Download and configure spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.8.5
)
FetchContent_MakeAvailable(spdlog)

# Download and configure CLI11
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v1.9.1
)
FetchContent_MakeAvailable(cli11)

# Download and configure hyprlang
ExternalProject_Add(
  hyprlang
  GIT_REPOSITORY https://github.com/hyprwm/hyprlang.git
  GIT_TAG main
  CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/hyprlang_install -DPKG_CONFIG_PATH=${CMAKE_BINARY_DIR}/hyprutils_install/lib/pkgconfig
  BUILD_COMMAND ${CMAKE_COMMAND} --build . --target install -j
  INSTALL_COMMAND ""
)

# Add the executable
add_executable(hyq src/main.cpp)

# Include hyprlang header
target_include_directories(hyq PRIVATE ${hyprlang_SOURCE_DIR}/include)

# Link the hyprlang library
add_dependencies(hyq hyprlang)
target_link_libraries(hyq PRIVATE ${CMAKE_BINARY_DIR}/hyprlang_install/lib/libhyprlang.so)

# Link nlohmann_json, spdlog, and CLI11 to the executable
target_link_libraries(hyq PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog CLI11::CLI11)
