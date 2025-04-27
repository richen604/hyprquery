#include "../build/_deps/nlohmann_json-src/single_include/nlohmann/json.hpp"
#include <CLI/CLI.hpp>
#include <any>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glob.h>
#include <iostream>
#include <map>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <wordexp.h>

// Include our compatibility header
#include "hyprlang_compat.hpp"

static Hyprlang::CConfig *pConfig = nullptr;
static std::string configDir = "";
static bool strictMode = false;

std::string expandEnvVars(const std::string &path) {
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

std::vector<std::filesystem::path> resolvePath(const std::string &path) {
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
      fsPath = std::filesystem::weakly_canonical(configDir / fsPath);
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
    spdlog::debug("PATHS: {}  ::: {}", configDir, p.string());
  }

  return paths;
}

void addConfigValuesFromSchema(Hyprlang::CConfig &config,
                               const std::string &schemaFilePath) {
  std::ifstream schemaFile(schemaFilePath);
  if (!schemaFile.is_open()) {
    std::cerr << "Failed to open schema file: " << schemaFilePath << std::endl;
    return;
  }

  nlohmann::json schemaJson;
  schemaFile >> schemaJson;

  for (const auto &option : schemaJson["hyprlang_schema"]) {
    if (!option.contains("value") || !option.contains("type") ||
        !option.contains("data")) {
      std::cerr << "Invalid schema format" << std::endl;
      continue;
    }

    std::string value = option["value"];
    std::string type = option["type"];
    std::any defaultValue;

    if (type == "INT" && option["data"].contains("default")) {
      config.addConfigValue(
          value.c_str(), (Hyprlang::INT)option["data"]["default"].get<int>());
    } else if (type == "FLOAT" && option["data"].contains("default")) {
      config.addConfigValue(
          value.c_str(),
          (Hyprlang::FLOAT)option["data"]["default"].get<float>());
    } else if ((type == "STRING_SHORT" || type == "STRING_LONG") &&
               option["data"].contains("default")) {
      config.addConfigValue(value.c_str(),
                            (Hyprlang::STRING)option["data"]["default"]
                                .get<std::string>()
                                .c_str());
    } else if (type == "BOOL" && option["data"].contains("default")) {
      config.addConfigValue(
          value.c_str(), (Hyprlang::INT)option["data"]["default"].get<bool>());
    } else if ((type == "GRADIENT" || type == "COLOR") &&
               option["data"].contains("default")) {
      config.addConfigValue(value.c_str(),
                            (Hyprlang::STRING)option["data"]["default"]
                                .get<std::string>()
                                .c_str());
    } else if (type == "VECTOR" && option["data"].contains("default")) {
      if (option["data"]["default"].is_array() &&
          option["data"]["default"].size() == 2) {
        Hyprlang::VEC2 vec;
        vec.x = option["data"]["default"][0].get<float>();
        vec.y = option["data"]["default"][1].get<float>();
        config.addConfigValue(value.c_str(), vec);
      }
    }
  }
}

static Hyprlang::CParseResult handleSource(const char *command,
                                           const char *rawpath) {
  // Path Validation
  if (strlen(rawpath) < 2) {
    Hyprlang::CParseResult result;
    result.setError("Invalid path length");
    return result;
  }

  // Path Expansion
  std::vector<std::filesystem::path> paths = resolvePath(rawpath);
  if (paths.empty()) {
    Hyprlang::CParseResult result;
    result.setError("No valid paths found");
    return result;
  }

  Hyprlang::CParseResult finalResult;

  // File Validation and Parsing
  for (const auto &path : paths) {
    if (!std::filesystem::is_regular_file(path)) {
      Hyprlang::CParseResult result;
      // result.setError("Path is not a regular file: " + path.string());
      return result;
    }

    spdlog::debug("Parsing file: {}", path.string());
    auto result = pConfig->parseFile(path.c_str());
    if (result.error) {
      return result;
    }

    // Update the current path to the directory of the parsed file
    configDir = path.parent_path().string();
  }

  return finalResult;
}

struct QueryResult {
  std::string key;
  std::string value;
  std::string type;
  std::vector<std::string> flags;
};

void printQueryResultAsJson(const QueryResult &result) {
  // In JSON mode, we will output NULL results
  nlohmann::json jsonResult;
  jsonResult["key"] = result.key;
  jsonResult["val"] = result.value;
  jsonResult["type"] = result.type;
  jsonResult["flags"] = result.flags;

  std::cout << jsonResult.dump(2) << std::endl;
}

void printQueryResultAsPlainText(const QueryResult &result) {
  // In plain text mode, don't output anything for NULL results
  if (result.type == "NULL") {
    spdlog::debug("Query result is NULL for key: {}", result.key);
    return;
  }

  std::cout << result.value << std::endl;
}

// Adding a function to parse variables directly from the config file
std::map<std::string, std::string>
parseVariablesFromFile(const std::string &filePath) {
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

        // Store in map
        variables[varName] = varValue;
        spdlog::debug("Found variable: {} = {}", varName, varValue);
      }
    }
  }

  return variables;
}

int main(int argc, char **argv, char **envp) {
  // Configure spdlog based on LOG_LEVEL environment variable
  const char *logLevelEnv = std::getenv("LOG_LEVEL");
  if (logLevelEnv) {
    std::string logLevelStr(logLevelEnv);
    if (logLevelStr == "debug") {
      spdlog::set_level(spdlog::level::debug);
    } else if (logLevelStr == "info") {
      spdlog::set_level(spdlog::level::info);
    } else if (logLevelStr == "warn") {
      spdlog::set_level(spdlog::level::warn);
    } else if (logLevelStr == "error") {
      spdlog::set_level(spdlog::level::err);
    } else if (logLevelStr == "critical") {
      spdlog::set_level(spdlog::level::critical);
    } else {
      spdlog::set_level(spdlog::level::info); // Default to info level
    }
  } else {
    spdlog::set_level(spdlog::level::info); // Default to info level
  }

  CLI::App app{"hyprparser - A configuration parser for hypr* config files"};

  std::string query;
  std::string configFilePath;
  std::string schemaFilePath;
  bool allowMissing = false;
  bool getDefaultKeys = false;
  bool strictMode = false;
  bool jsonOutput = false;
  bool followSource = false;

  app.add_option("--query", query, "Query to execute")->required();
  app.add_option("config_file", configFilePath, "Configuration file")
      ->required();
  app.add_option("--schema", schemaFilePath, "Schema file");
  app.add_flag("--allow-missing", allowMissing, "Allow missing values");
  app.add_flag("--get-defaults", getDefaultKeys, "Get default keys");
  app.add_flag("--strict", strictMode, "Enable strict mode");
  app.add_flag("--json,-j", jsonOutput, "Output result in JSON format");
  app.add_flag("--source,-s", "follow the source command");

  CLI11_PARSE(app, argc, argv);

  auto resolvedPaths = resolvePath(configFilePath);
  if (resolvedPaths.empty()) {
    std::cerr << "Error: Could not resolve configuration file path: "
              << configFilePath << std::endl;
    return 1;
  }

  configFilePath = resolvedPaths.front().string();

  // Check if the input config file exists
  if (!std::filesystem::exists(configFilePath)) {
    std::cerr << "Error: Configuration file does not exist: " << configFilePath
              << std::endl;
    return 1;
  }

  configDir = std::filesystem::path(configFilePath).parent_path().string();
  if (!schemaFilePath.empty()) {
    schemaFilePath = resolvePath(schemaFilePath).front().string();
  }

  // Parse variables from the config file before initializing Hyprlang
  std::map<std::string, std::string> variables =
      parseVariablesFromFile(configFilePath);

  Hyprlang::SConfigOptions options;
  options = {.verifyOnly = getDefaultKeys ? 1 : 0, .allowMissingConfig = 1};
  Hyprlang::CConfig config(configFilePath.c_str(), options);
  pConfig = &config;

  if (!schemaFilePath.empty()) {
    addConfigValuesFromSchema(config, schemaFilePath);
  }

  if (followSource) {
    config.registerHandler(
        [](const char *command, const char *value) -> Hyprlang::CParseResult {
          return handleSource(command, value);
        },
        "source", {.allowFlags = false});
  }

  // Make sure the variable will be parsed if it's referenced in the config
  if (!query.empty() && query[0] != '$') {
    config.addConfigValue(query.c_str(), (Hyprlang::STRING) "UNSET");
  }

  config.commence();

  const auto PARSERESULT = config.parse();
  if (PARSERESULT.error) {
    spdlog::debug("Parse error: {}", PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }

  QueryResult result;
  result.key = query;

  // Check if the query is for a variable (starts with $)
  if (!query.empty() && query[0] == '$') {
    // First try to get from our parsed variables
    auto varIt = variables.find(query);
    if (varIt != variables.end()) {
      result.value = varIt->second;
      result.type = "STRING"; // Variables are typically treated as strings
    } else {
      // Fallback to Hyprlang's getConfigValue if not found in our map
      std::any value = HyprlangCompat::getConfigValue(&config, query.c_str());
      result.value = HyprlangCompat::convertValueToString(value);
      result.type = HyprlangCompat::getValueTypeName(value);
    }
  } else {
    // Regular config value lookup (non-variable)
    std::any value = HyprlangCompat::getConfigValue(&config, query.c_str());
    result.value = HyprlangCompat::convertValueToString(value);
    result.type = HyprlangCompat::getValueTypeName(value);
  }

  if (jsonOutput) {
    printQueryResultAsJson(result);
  } else {
    printQueryResultAsPlainText(result);
  }

  // If the result is NULL, log it at debug level and return non-zero exit code
  if (result.type == "NULL") {
    spdlog::debug("Query '{}' returned NULL", query);
    return 1;
  }
  return 0;
}
