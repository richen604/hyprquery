#pragma once

#include <filesystem>
#include <hyprlang.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace hyprquery {

// Forward declarations
class SourceHandler {
public:
  // Register the source handler with a config instance
  static void registerHandler(Hyprlang::CConfig *config);

  // Source handler function
  static Hyprlang::CParseResult handleSource(const char *command,
                                             const char *value);

  // Set the current directory for path resolution
  static void setConfigDir(const std::string &dir);

  // Get the current config directory
  static std::string getConfigDir();

  // Path utility functions
  static std::vector<std::filesystem::path>
  resolvePath(const std::string &path);
  static std::string expandEnvVars(const std::string &path);

  // Variable handling
  static std::map<std::string, std::string>
  parseVariablesFromFile(const std::string &filePath);

  // Get a variable by name
  static std::optional<std::string> getVariable(const std::string &name);

  // Check if the source handler has been initialized
  static bool isInitialized();

private:
  static Hyprlang::CConfig *s_pConfig;
  static std::string s_configDir;
  static std::map<std::string, std::string> s_allVariables;
  static bool s_initialized;
};

} // namespace hyprquery