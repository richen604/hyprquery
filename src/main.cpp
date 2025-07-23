#include "ConfigUtils.hpp"
#include "SourceHandler.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <functional>
#include <hyprlang.hpp>
#include <nlohmann/json.hpp>
#include <regex>
#include <spdlog/spdlog.h>

static Hyprlang::CConfig *pConfig = nullptr;

struct QueryResult {
  std::string key;
  std::string value;
  std::string type;
  std::vector<std::string> flags;
};

int main(int argc, char **argv, char **envp) {
  CLI::App app{"hyprquery - A configuration parser for hypr* config files"};

  std::vector<std::string> queries;
  std::string configFilePath;
  std::string schemaFilePath;
  bool allowMissing = false;
  bool getDefaultKeys = false;
  bool strictMode = false;
  bool jsonOutput = false;
  bool followSource = false;
  bool debugLogging = false;
  std::string delimiter = "\n";
  std::vector<std::string> expectedTypes;
  std::vector<std::string> expectedRegexes;

  app.add_option("--query,-Q", queries,
                 "Query to execute (can be specified multiple times)")
      ->required()
      ->take_all();
  app.add_option("config_file", configFilePath, "Configuration file")
      ->required();
  app.add_option("--schema", schemaFilePath, "Schema file");
  app.add_flag("--allow-missing", allowMissing, "Allow missing values");
  app.add_flag("--get-defaults", getDefaultKeys, "Get default keys");
  app.add_flag("--strict", strictMode, "Enable strict mode");
  app.add_flag("--json,-j", jsonOutput, "Output result in JSON format");
  app.add_flag("--source,-s", followSource, "Follow the source command");
  app.add_flag("--debug", debugLogging, "Enable debug logging");
  app.add_option("--delimiter,-D", delimiter,
                 "Delimiter for plain output (default: newline)");
  app.add_option(
      "--type,-T", expectedTypes,
      "Expected type for each query (optional, matches order of -Q)");
  app.add_option(
      "--expect-regex,-R", expectedRegexes,
      "Expected regex for each query (optional, matches order of -Q)");

  bool variableSearch = false;

  CLI11_PARSE(app, argc, argv);

  for (const auto &q : queries) {
    if (!q.empty() && q[0] == '$') {
      variableSearch = true;
      break;
    }
  }

  if (debugLogging) {
    spdlog::set_level(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::off);
  }

  configFilePath = hyprquery::ConfigUtils::normalizePath(configFilePath);
  auto resolvedPaths = hyprquery::SourceHandler::resolvePath(configFilePath);
  if (resolvedPaths.empty()) {
    std::cerr << "Error: Could not resolve configuration file path: "
              << configFilePath << std::endl;
    return 1;
  }

  configFilePath = resolvedPaths.front().string();

  if (!std::filesystem::exists(configFilePath)) {
    std::cerr << "Error: Configuration file does not exist: " << configFilePath
              << std::endl;
    return 1;
  }

  hyprquery::SourceHandler::setConfigDir(
      std::filesystem::path(configFilePath).parent_path().string());

  if (!schemaFilePath.empty()) {
    schemaFilePath = hyprquery::ConfigUtils::normalizePath(schemaFilePath);
    auto resolvedSchemaPath =
        hyprquery::SourceHandler::resolvePath(schemaFilePath);
    if (!resolvedSchemaPath.empty()) {
      schemaFilePath = resolvedSchemaPath.front().string();
    }

    if (!std::filesystem::exists(schemaFilePath)) {
      std::cerr << "Error: Schema file does not exist: " << schemaFilePath
                << std::endl;
      return 1;
    }
  }

  Hyprlang::SConfigOptions options;
  options = {.verifyOnly = static_cast<bool>(getDefaultKeys ? 1 : 0),
             .allowMissingConfig = static_cast<bool>(1)};

  std::hash<std::string> hasher;

  std::string configStream;
  if (variableSearch) {
    if (debugLogging)
      spdlog::debug("[variable-search] Enabled");
    std::ifstream src(configFilePath);
    std::ostringstream dst;
    dst << src.rdbuf();
    for (size_t i = 0; i < queries.size(); ++i) {
      if (!queries[i].empty() && queries[i][0] == '$') {
        std::string dynKey = "Dynamic_" + std::to_string(hasher(queries[i]));
        std::string dynLine =
            std::string("\n") + dynKey + "=" + queries[i] + "\n";
        dst << dynLine;
        if (debugLogging)
          spdlog::debug(std::string("[variable-search] Injecting line: ") +
                        dynLine.substr(1, dynLine.size() - 2));
      }
    }
    src.close();
    configStream = dst.str();
    configFilePath = configStream;
    options.pathIsStream = true;
  }

  Hyprlang::CConfig config(configFilePath.c_str(), options);
  pConfig = &config;

  config.addConfigValue("int", (Hyprlang::INT)0);
  config.addConfigValue("ok", (Hyprlang::INT)0);
  config.addConfigValue("key", (Hyprlang::STRING) "");

  if (!schemaFilePath.empty()) {
    hyprquery::ConfigUtils::addConfigValuesFromSchema(config, schemaFilePath);
  }

  if (followSource) {
    if (debugLogging)
      spdlog::debug("Registering source handler");
    hyprquery::SourceHandler::registerHandler(pConfig);
  }

  std::vector<std::string> dynamicVars(queries.size());
  for (size_t i = 0; i < queries.size(); ++i) {
    if (variableSearch && !queries[i].empty() && queries[i][0] == '$') {
      std::string dynKey = "Dynamic_" + std::to_string(hasher(queries[i]));
      dynamicVars[i] = dynKey;
      config.addConfigValue(dynKey.c_str(), (Hyprlang::STRING) "");
      if (debugLogging)
        spdlog::debug(std::string("[variable-search] Mapping query '") +
                      queries[i] + "' to injected key '" + dynKey + "'");
    } else {
      dynamicVars[i] = queries[i];
      config.addConfigValue(queries[i].c_str(), (Hyprlang::STRING) "");
    }
  }

  config.commence();

  const auto PARSERESULT = config.parse();
  if (PARSERESULT.error) {
    if (debugLogging)
      spdlog::debug(std::string("Parse error: ") + PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }

  int nullCount = 0;
  if (jsonOutput) {
    nlohmann::json jsonArr = nlohmann::json::array();
    for (size_t i = 0; i < queries.size(); ++i) {
      QueryResult result;
      result.key = queries[i];
      std::string lookupKey =
          (variableSearch && !queries[i].empty() && queries[i][0] == '$')
              ? dynamicVars[i]
              : queries[i];
      if (debugLogging)
        spdlog::debug(std::string("[variable-search] Lookup key for query '") +
                      queries[i] + "' is '" + lookupKey + "'");
      if (debugLogging && variableSearch) {
        spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                      "' lookup key: '" + lookupKey + "'");
      }
      std::any value = pConfig->getConfigValue(lookupKey.c_str());
      result.value = hyprquery::ConfigUtils::convertValueToString(value);
      result.type = hyprquery::ConfigUtils::getValueTypeName(value);

      if (!expectedTypes.empty() && i < expectedTypes.size() &&
          !expectedTypes[i].empty()) {
        if (result.type != expectedTypes[i]) {
          result.value = "";
          result.type = "NULL";
        }
      }

      if (!expectedRegexes.empty() && i < expectedRegexes.size() &&
          !expectedRegexes[i].empty()) {
        try {
          std::regex rx(expectedRegexes[i]);
          if (!std::regex_match(result.value, rx)) {
            result.value = "";
            result.type = "NULL";
          }
        } catch (const std::regex_error &) {

          result.value = "";
          result.type = "NULL";
        }
      }

      jsonArr.push_back({{"key", result.key},
                         {"val", result.value},
                         {"type", result.type},
                         {"flags", result.flags}});
      if (result.type == "NULL") {
        if (debugLogging)
          spdlog::debug(std::string("Query '") + queries[i] +
                        "' returned NULL");
        if (debugLogging && variableSearch) {
          spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                        "' (lookup: '" + lookupKey + ") returned NULL");
        }
        nullCount++;
      } else if (debugLogging && variableSearch) {
        spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                      "' (lookup: '" + lookupKey + ") returned: '" +
                      result.value + "'");
      }
    }
    std::cout << jsonArr.dump(2) << std::endl;
  } else {
    std::vector<std::string> outputs;
    for (size_t i = 0; i < queries.size(); ++i) {
      QueryResult result;
      result.key = queries[i];
      std::string lookupKey =
          (variableSearch && !queries[i].empty() && queries[i][0] == '$')
              ? dynamicVars[i]
              : queries[i];
      if (debugLogging)
        spdlog::debug(std::string("[variable-search] Lookup key for query '") +
                      queries[i] + "' is '" + lookupKey + "'");
      if (debugLogging && variableSearch) {
        spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                      "' lookup key: '" + lookupKey + "'");
      }
      std::any value = pConfig->getConfigValue(lookupKey.c_str());
      result.value = hyprquery::ConfigUtils::convertValueToString(value);
      result.type = hyprquery::ConfigUtils::getValueTypeName(value);

      if (!expectedTypes.empty() && i < expectedTypes.size() &&
          !expectedTypes[i].empty()) {
        if (result.type != expectedTypes[i]) {
          result.value = "";
          result.type = "NULL";
        }
      }

      if (!expectedRegexes.empty() && i < expectedRegexes.size() &&
          !expectedRegexes[i].empty()) {
        try {
          std::regex rx(expectedRegexes[i]);
          if (!std::regex_match(result.value, rx)) {
            result.value = "";
            result.type = "NULL";
          }
        } catch (const std::regex_error &) {

          result.value = "";
          result.type = "NULL";
        }
      }

      outputs.push_back(result.value);
      if (result.type == "NULL") {
        if (debugLogging)
          spdlog::debug(std::string("Query '") + queries[i] +
                        "' returned NULL");
        if (debugLogging && variableSearch) {
          spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                        "' (lookup: '" + lookupKey + ") returned NULL");
        }
        nullCount++;
      } else if (debugLogging && variableSearch) {
        spdlog::debug(std::string("[variable-search] Query '") + queries[i] +
                      "' (lookup: '" + lookupKey + ") returned: '" +
                      result.value + "'");
      }
    }

    for (size_t i = 0; i < outputs.size(); ++i) {
      std::cout << outputs[i];
      if (i + 1 < outputs.size())
        std::cout << delimiter;
    }
    std::cout << std::endl;
  }

  return nullCount > 0 ? 1 : 0;
}
