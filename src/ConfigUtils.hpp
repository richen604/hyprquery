#pragma once

#include <any>
#include <fstream>
#include <hyprlang.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace hyprquery {

class ConfigUtils {
public:
  // Add configuration values from a JSON schema file
  static void addConfigValuesFromSchema(Hyprlang::CConfig &config,
                                        const std::string &schemaFilePath);

  // Utility functions for working with config values
  static std::string convertValueToString(const std::any &value);
  static std::string getValueTypeName(const std::any &value);

  // Handle variables
  static std::optional<int64_t> configStringToInt(const std::string &str);

  // Get workspace information from string
  static std::pair<int64_t, std::string>
  getWorkspaceIDNameFromString(const std::string &str);

  // Clean command for workspace
  static std::optional<std::string>
  cleanCmdForWorkspace(const std::string &name, const std::string &cmd);
};

} // namespace hyprquery