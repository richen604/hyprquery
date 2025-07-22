#pragma once

#include <filesystem>
#include <hyprlang.hpp>
#include <spdlog/spdlog.h>
#include <string>
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

  // Check if the source handler has been initialized
  static bool isInitialized();

private:
  static Hyprlang::CConfig *s_pConfig;
  static std::string s_configDir;
  static bool s_initialized;
};

} // namespace hyprquery