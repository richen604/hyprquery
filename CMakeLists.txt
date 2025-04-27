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

# Set binary output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

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
include(GNUInstallDirs)
include(CheckCXXSourceCompiles)

# =================== Options ===================
option(USE_SYSTEM_HYPRLANG "Use system installed hyprlang when available" ON)
option(HYPRLANG_VERSION "Specific hyprlang version to use" "v0.6.0")
option(STRICT_MODE "Enable strict mode checks" OFF)

# =================== Dependencies ===================
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.3.2
)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.11.0
)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

FetchContent_MakeAvailable(cli11 spdlog json)

# =================== Feature Testing ===================
# Check if std::print is available
check_cxx_source_compiles("
#include <print>
int main() {
    std::print(\"Hello World\");
    return 0;
}
" HAS_STD_PRINT)

if(HAS_STD_PRINT)
  add_compile_definitions(HAS_STD_PRINT)
endif()

# =================== Hyprlang Handling ===================
# First try to find system installed hyprlang
if(USE_SYSTEM_HYPRLANG)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(HYPRLANG IMPORTED_TARGET hyprlang>=0.6.0)
    
    # If we found hyprlang, check the exact version
    if(HYPRLANG_FOUND)
      execute_process(
        COMMAND bash -c "pkg-config --modversion hyprlang"
        OUTPUT_VARIABLE DETECTED_HYPRLANG_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      message(STATUS "Found system hyprlang: ${DETECTED_HYPRLANG_VERSION}")
      
      # Check if version is high enough to require C++23 features
      if(DETECTED_HYPRLANG_VERSION VERSION_GREATER_EQUAL "0.6.1")
        message(STATUS "Detected hyprlang version uses C++23 features, adding compatibility layer")
        add_compile_definitions(HYPRLANG_CPP23)
      endif()
    endif()
  endif()
endif()

if(HYPRLANG_FOUND)
  message(STATUS "Using system hyprlang")
  # Use the system installed version
  set(HYPRLANG_INCLUDE_DIR ${HYPRLANG_INCLUDE_DIRS})
  set(HYPRLANG_LIBRARY ${HYPRLANG_LIBRARIES})
  add_compile_definitions(USE_SYSTEM_HYPRLANG)
else()
  message(STATUS "System hyprlang not found, building from source (${HYPRLANG_VERSION})")
  # Build hyprlang from source
  ExternalProject_Add(
    hyprlang
    GIT_REPOSITORY https://github.com/hyprwm/hyprlang.git
    GIT_TAG ${HYPRLANG_VERSION}
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/hyprlang_install
    BUILD_COMMAND ${CMAKE_COMMAND} --build . --target install -j
    INSTALL_COMMAND ""
  )
  set(HYPRLANG_INCLUDE_DIR "${CMAKE_BINARY_DIR}/hyprlang_install/include")
  set(HYPRLANG_LIBRARY "${CMAKE_BINARY_DIR}/hyprlang_install/lib/libhyprlang.so")
endif()

# =================== Main Executable ===================
add_executable(hyq src/main.cpp)

# Set binary output directory and name
set_target_properties(hyq PROPERTIES RUNTIME_OUTPUT_NAME "hyq")

# Include hyprlang header
target_include_directories(hyq PRIVATE ${HYPRLANG_INCLUDE_DIR})

# Strict mode checks
if(STRICT_MODE)
  add_compile_definitions(STRICT_MODE)
endif()

# Link dependencies
if(NOT HYPRLANG_FOUND)
  add_dependencies(hyq hyprlang)
endif()

target_link_libraries(hyq PRIVATE 
  ${HYPRLANG_LIBRARY}
  nlohmann_json::nlohmann_json 
  spdlog::spdlog 
  CLI11::CLI11
)

# =================== Install Targets ===================
install(TARGETS hyq
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install documentation
install(FILES 
  ${CMAKE_SOURCE_DIR}/README.md
  ${CMAKE_SOURCE_DIR}/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
