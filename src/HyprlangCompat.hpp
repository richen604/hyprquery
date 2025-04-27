#pragma once

#include <any>
#include <string>

// Try to include system hyprlang first, fall back to local install
#ifdef USE_SYSTEM_HYPRLANG
#include <hyprlang.hpp>
#else
// Try standard include path first
#if __has_include(<hyprlang.hpp>)
#include <hyprlang.hpp>
// Then try relative paths for local builds
#elif __has_include("../build/hyprlang_install/include/hyprlang.hpp")
#include "../build/hyprlang_install/include/hyprlang.hpp"
#elif __has_include("../hyprlang/include/hyprlang.hpp")
#include "../hyprlang/include/hyprlang.hpp"
#else
#error "Could not find hyprlang.hpp - please install it or specify its location"
#endif
#endif

// Define a default ABI version if not provided
#ifndef HYPRLANG_ABI_VERSION
#define HYPRLANG_ABI_VERSION 1
#endif

namespace hyprquery {

// Compatibility layer for working with Hyprlang
class HyprlangCompat {
public:
  // Get a configuration value from a config object
  static std::any getConfigValue(Hyprlang::CConfig *config,
                                 const std::string &name);

  // Get a configuration value as a specific type
  template <typename T>
  static T getConfigValueAs(Hyprlang::CConfig *config, const std::string &name,
                            const T &defaultValue = T{}) {
    auto value = getConfigValue(config, name);
    try {
      return std::any_cast<T>(value);
    } catch (const std::bad_any_cast &) {
      return defaultValue;
    }
  }

  // Convert a config value to a string
  static std::string convertValueToString(const std::any &value);

  // Get the name of the value type
  static std::string getValueTypeName(const std::any &value);
};

} // namespace hyprquery