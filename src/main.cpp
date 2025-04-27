#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "ConfigUtils.hpp"
#include "HyprlangCompat.hpp"
#include "SourceHandler.hpp"

static Hyprlang::CConfig *pConfig = nullptr;

struct QueryResult {
  std::string key;
  std::string value;
  std::string type;
  std::vector<std::string> flags;
};

void printQueryResultAsJson(const QueryResult &result);
void printQueryResultAsPlainText(const QueryResult &result);

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

  CLI::App app{"hyprquery - A configuration parser for hypr* config files"};

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
  app.add_flag("--source,-s", followSource, "Follow the source command");

  CLI11_PARSE(app, argc, argv);

  // Resolve config file path
  configFilePath = hyprquery::ConfigUtils::normalizePath(configFilePath);
  auto resolvedPaths = hyprquery::SourceHandler::resolvePath(configFilePath);
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

  // Set the config directory based on the first file
  hyprquery::SourceHandler::setConfigDir(
      std::filesystem::path(configFilePath).parent_path().string());

  // Handle schema path resolution
  if (!schemaFilePath.empty()) {
    schemaFilePath = hyprquery::ConfigUtils::normalizePath(schemaFilePath);
    auto resolvedSchemaPath =
        hyprquery::SourceHandler::resolvePath(schemaFilePath);
    if (!resolvedSchemaPath.empty()) {
      schemaFilePath = resolvedSchemaPath.front().string();
    }

    // Verify schema file exists
    if (!std::filesystem::exists(schemaFilePath)) {
      std::cerr << "Error: Schema file does not exist: " << schemaFilePath
                << std::endl;
      return 1;
    }
  }

  // Parse variables from the config file before initializing Hyprlang
  std::map<std::string, std::string> variables =
      hyprquery::SourceHandler::parseVariablesFromFile(configFilePath);

  // Initialize Hyprlang config
  Hyprlang::SConfigOptions options;
  options = {.verifyOnly = getDefaultKeys ? 1 : 0, .allowMissingConfig = 1};
  Hyprlang::CConfig config(configFilePath.c_str(), options);
  pConfig = &config;

  // Register common config values we expect to see
  config.addConfigValue("int", (Hyprlang::INT)0);
  config.addConfigValue("ok", (Hyprlang::INT)0);
  config.addConfigValue("key", (Hyprlang::STRING) "");

  // Add schema-based config values if schema provided
  if (!schemaFilePath.empty()) {
    hyprquery::ConfigUtils::addConfigValuesFromSchema(config, schemaFilePath);
  }

  // Register the source handler if --source flag is provided
  if (followSource) {
    spdlog::debug("Registering source handler");
    hyprquery::SourceHandler::registerHandler(pConfig);
  }

  // Make sure the variable will be parsed if it's referenced in the config
  if (!query.empty() && query[0] != '$') {
    config.addConfigValue(query.c_str(), (Hyprlang::STRING) "UNSET");
  }

  // Start the config parsing process
  config.commence();

  // Parse the config
  const auto PARSERESULT = config.parse();
  if (PARSERESULT.error) {
    spdlog::debug("Parse error: {}", PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }

  // Initialize result structure
  QueryResult result;
  result.key = query;

  // Check if the query is for a variable (starts with $)
  if (!query.empty() && query[0] == '$') {
    // First try to get from our local variables (from main config file)
    auto varIt = variables.find(query);
    if (varIt != variables.end()) {
      result.value = varIt->second;
      result.type = "STRING"; // Variables are typically treated as strings
    } else {
      // Check if we can find the variable in the global variable map
      auto globalVar = hyprquery::SourceHandler::getVariable(query);
      if (globalVar.has_value()) {
        result.value = globalVar.value();
        result.type = "STRING";
        spdlog::debug("Found variable in sourced file: {} = {}", query,
                      result.value);
      } else {
        // Fallback to Hyprlang's config value lookup
        std::any value =
            hyprquery::HyprlangCompat::getConfigValue(pConfig, query.c_str());
        result.value = hyprquery::HyprlangCompat::convertValueToString(value);
        result.type = hyprquery::HyprlangCompat::getValueTypeName(value);
      }
    }
  } else {
    // Regular config value lookup (non-variable)
    std::any value =
        hyprquery::HyprlangCompat::getConfigValue(pConfig, query.c_str());
    result.value = hyprquery::HyprlangCompat::convertValueToString(value);
    result.type = hyprquery::HyprlangCompat::getValueTypeName(value);
  }

  // Output the result
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
