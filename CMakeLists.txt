cmake_minimum_required(VERSION 3.10)
project(HyprlangProject)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
include_directories(/usr/include)

# Add FetchContent module
include(FetchContent)

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

# Add the executable
add_executable(hyq src/main.cpp)

# Link the hyprlang library
target_link_libraries(hyq PRIVATE hyprlang)

# Link nlohmann_json, spdlog, and CLI11 to the executable
target_link_libraries(hyq PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog CLI11::CLI11)
