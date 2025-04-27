#pragma once

#include <any>
#include <hyprlang.hpp>
#include <string>

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