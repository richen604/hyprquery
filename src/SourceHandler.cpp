#include "SourceHandler.hpp"
#include <cstring>
#include <fstream>
#include <glob.h>
#include <memory>
#include <wordexp.h>

namespace hyprquery {

// Initialize static members
Hyprlang::CConfig *SourceHandler::s_pConfig = nullptr;
std::string SourceHandler::s_configDir = "";
std::map<std::string, std::string> SourceHandler::s_allVariables;
bool SourceHandler::s_initialized = false;

void SourceHandler::setConfigDir(const std::string &dir) { s_configDir = dir; }

std::string SourceHandler::getConfigDir() { return s_configDir; }

bool SourceHandler::isInitialized() { return s_initialized; }

std::string SourceHandler::expandEnvVars(const std::string &path) {
  wordexp_t p;
  std::string result = path;

  // Handle wordexp() special cases
  // Replace $ with $$ for any $ that precedes { to avoid command substitution
  size_t pos = 0;
  while ((pos = result.find("${", pos)) != std::string::npos) {
    result.insert(pos + 1, "$");
    pos += 3; // Skip past the inserted $
  }

  // Use wordexp to handle environment variable expansion and tilde expansion
  if (wordexp(result.c_str(), &p, WRDE_NOCMD) == 0) {
    if (p.we_wordc > 0 && p.we_wordv[0] != nullptr) {
      result = p.we_wordv[0];
    }
    wordfree(&p);
  } else {
    spdlog::warn("Failed to expand environment variables in path: {}", path);
  }

  return result;
}

std::vector<std::filesystem::path>
SourceHandler::resolvePath(const std::string &path) {
  std::string expandedPath = expandEnvVars(path);
  std::vector<std::filesystem::path> paths;

  spdlog::debug("Expanded path: {}", expandedPath);

  // Handle case where the path doesn't exist
  if (expandedPath.empty()) {
    spdlog::error("Path is empty after expansion: {}", path);
    return paths;
  }

  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_t));

  int ret = glob(expandedPath.c_str(), GLOB_TILDE | GLOB_BRACE, nullptr,
                 &glob_result);

  if (ret != 0) {
    if (ret == GLOB_NOMATCH) {
      spdlog::warn("No matches found for path: {}", expandedPath);
    } else {
      spdlog::error("Glob error for pattern: {}", expandedPath);
    }
    // Even if glob fails, try to use the path directly as a fallback
    std::filesystem::path fallbackPath(expandedPath);
    if (std::filesystem::exists(fallbackPath.parent_path())) {
      paths.push_back(fallbackPath);
    }
    globfree(&glob_result);
    return paths;
  }

  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    std::string pathStr = glob_result.gl_pathv[i];
    std::filesystem::path fsPath(pathStr);
    if (fsPath.is_relative() || fsPath.string().find("./") == 0) {
      fsPath = std::filesystem::weakly_canonical(s_configDir / fsPath);
    } else {
      fsPath = std::filesystem::weakly_canonical(fsPath);
    }
    if (std::filesystem::exists(fsPath.parent_path())) {
      paths.push_back(fsPath);
    } else {
      spdlog::warn("Directory does not exist: {}",
                   fsPath.parent_path().string());
    }
  }
  globfree(&glob_result);

  spdlog::debug("Resolved paths: ");
  for (const auto &p : paths) {
    spdlog::debug("PATHS: {}  ::: {}", s_configDir, p.string());
  }

  return paths;
}

std::map<std::string, std::string>
SourceHandler::parseVariablesFromFile(const std::string &filePath) {
  std::map<std::string, std::string> variables;
  std::ifstream configFile(filePath);

  if (!configFile.is_open()) {
    spdlog::error("Failed to open config file for variable parsing: {}",
                  filePath);
    return variables;
  }

  std::string line;
  while (std::getline(configFile, line)) {
    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t"));

    // Look for variable definitions ($VAR=value)
    if (line.size() > 0 && line[0] == '$') {
      size_t equalPos = line.find('=');
      if (equalPos != std::string::npos) {
        std::string varName = line.substr(0, equalPos);
        std::string varValue = line.substr(equalPos + 1);

        // Trim whitespace from value
        varValue.erase(0, varValue.find_first_not_of(" \t"));
        varValue.erase(varValue.find_last_not_of(" \t") + 1);

        // Store in maps
        variables[varName] = varValue;
        s_allVariables[varName] = varValue; // Store in the global map too
        spdlog::debug("Found variable: {} = {}", varName, varValue);
      }
    }
  }

  return variables;
}

std::optional<std::string> SourceHandler::getVariable(const std::string &name) {
  auto it = s_allVariables.find(name);
  if (it != s_allVariables.end())
    return it->second;
  return std::nullopt;
}

Hyprlang::CParseResult SourceHandler::handleSource(const char *command,
                                                   const char *rawpath) {
  Hyprlang::CParseResult result;
  std::string path = rawpath;

  if (path.length() < 2) {
    result.setError("source= path too short or empty");
    return result;
  }

  // Save the current config directory
  std::string configDirBackup = s_configDir;

  // Path Expansion with glob
  std::unique_ptr<glob_t, void (*)(glob_t *)> glob_buf{
      static_cast<glob_t *>(calloc(1, sizeof(glob_t))), [](glob_t *g) {
        if (g) {
          globfree(g);
          free(g);
        }
      }};

  // Get absolute path
  std::string absolutePath = path;
  if (path[0] == '~') {
    const char *home = getenv("HOME");
    if (home)
      absolutePath = std::string(home) + path.substr(1);
  } else if (path[0] != '/') {
    // Handle relative paths
    absolutePath = s_configDir + "/" + path;
  }

  spdlog::debug("Source: resolving path pattern: {}", absolutePath);

  if (glob(absolutePath.c_str(), GLOB_TILDE, nullptr, glob_buf.get()) != 0) {
    globfree(glob_buf.get());
    result.setError(
        ("source= found no matching files for pattern: " + path).c_str());
    return result;
  }

  std::string errorsFromParsing;

  // Process each matching file
  for (size_t i = 0; i < glob_buf->gl_pathc; i++) {
    std::string filepath = glob_buf->gl_pathv[i];

    if (!std::filesystem::is_regular_file(filepath)) {
      if (std::filesystem::exists(filepath)) {
        spdlog::debug("Source: skipping non-file {}", filepath);
        continue;
      }

      spdlog::error("Source: file doesn't exist: {}", filepath);
      if (errorsFromParsing.empty()) {
        errorsFromParsing = "source= file doesn't exist: " + filepath;
      }
      continue;
    }

    spdlog::debug("Parsing source file: {}", filepath);

    // Set the config directory to the parent of the file we're parsing
    s_configDir = std::filesystem::path(filepath).parent_path().string();

    // Parse variables from the sourced file
    auto sourcedVariables = parseVariablesFromFile(filepath);

    // Parse the file
    auto parseResult = s_pConfig->parseFile(filepath.c_str());

    if (parseResult.error && errorsFromParsing.empty()) {
      errorsFromParsing = parseResult.getError();
    }
  }

  // Restore the original directory context
  s_configDir = configDirBackup;

  if (!errorsFromParsing.empty()) {
    result.setError(errorsFromParsing.c_str());
  }

  return result;
}

void SourceHandler::registerHandler(Hyprlang::CConfig *config) {
  s_pConfig = config;
  s_initialized = true;

  Hyprlang::SHandlerOptions options;
  options.allowFlags = false;

  config->registerHandler(
      [](const char *command, const char *value) -> Hyprlang::CParseResult {
        return SourceHandler::handleSource(command, value);
      },
      "source", options);

  spdlog::debug("Registered source handler");
}

} // namespace hyprquery