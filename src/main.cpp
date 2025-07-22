#include <CLI/CLI.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "ConfigUtils.hpp"
#include "SourceHandler.hpp"
#include <hyprlang.hpp>  // Direct include from the built library

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
  CLI::App app{"hyprquery - A configuration parser for hypr* config files"};

  std::string query;
  std::string configFilePath;
  std::string schemaFilePath;
  bool allowMissing = false;
  bool getDefaultKeys = false;
  bool strictMode = false;
  bool jsonOutput = false;
  bool followSource = false;
  bool debugLogging = false;

  app.add_option("--query", query, "Query to execute")->required();
  app.add_option("config_file", configFilePath, "Configuration file")
      ->required();
  app.add_option("--schema", schemaFilePath, "Schema file");
  app.add_flag("--allow-missing", allowMissing, "Allow missing values");
  app.add_flag("--get-defaults", getDefaultKeys, "Get default keys");
  app.add_flag("--strict", strictMode, "Enable strict mode");
  app.add_flag("--json,-j", jsonOutput, "Output result in JSON format");
  app.add_flag("--source,-s", followSource, "Follow the source command");
  app.add_flag("--debug", debugLogging, "Enable debug logging");

  CLI11_PARSE(app, argc, argv);

  // Set logging level based on --debug flag
  if (debugLogging) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::off);
  }

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

  // Initialize Hyprlang config
  Hyprlang::SConfigOptions options;
  options = {
    .verifyOnly = static_cast<bool>(getDefaultKeys ? 1 : 0),
    .allowMissingConfig = static_cast<bool>(1)
  };

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
    if (debugLogging) spdlog::debug("Registering source handler");
    hyprquery::SourceHandler::registerHandler(pConfig);
  }

  // Make sure the variable will be parsed if it's referenced in the config
  config.addConfigValue(query.c_str(), (Hyprlang::STRING) "UNSET");

  // Start the config parsing process
  config.commence();

  // Parse the config
  const auto PARSERESULT = config.parse();
  if (PARSERESULT.error) {
    if (debugLogging) spdlog::debug(std::string("Parse error: ") + PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }

  // Initialize result structure
  QueryResult result;
  result.key = query;

  // Always use Hyprlang's config value lookup for all queries
  std::any value = pConfig->getConfigValue(query.c_str());
  result.value = hyprquery::ConfigUtils::convertValueToString(value);
  result.type = hyprquery::ConfigUtils::getValueTypeName(value);

  // Output the result
  if (jsonOutput) {
    printQueryResultAsJson(result);
  } else {
    printQueryResultAsPlainText(result);
  }

  // If the result is NULL, log it at debug level and return non-zero exit code
  if (result.type == "NULL") {
    if (debugLogging) spdlog::debug(std::string("Query '") + query + "' returned NULL");
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
    // Only log if debugLogging is set, but we don't have access here, so skip log
    return;
  }

  std::cout << result.value << std::endl;
}
