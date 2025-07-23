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

void prepareConfig(const std::vector<hyprquery::QueryInput> &queries,
                   std::string &configFilePath,
                   Hyprlang::SConfigOptions &options,
                   std::vector<std::string> &dynamicVars, bool debugLogging) {
  std::hash<std::string> hasher;
  bool variableSearch = false;
  for (const auto &q : queries) {
    if (q.isDynamicVariable) {
      variableSearch = true;
      break;
    }
  }
  std::string configStream;
  if (variableSearch) {
    if (debugLogging)
      spdlog::debug("[variable-search] Enabled");
    std::ifstream src(configFilePath);
    std::ostringstream dst;
    dst << src.rdbuf();
    for (size_t i = 0; i < queries.size(); ++i) {
      if (queries[i].isDynamicVariable) {
        std::string dynKey =
            "Dynamic_" + std::to_string(hasher(queries[i].query));
        std::string dynLine = "\n" + dynKey + "=" + queries[i].query + "\n";
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
  dynamicVars.resize(queries.size());

  pConfig = new Hyprlang::CConfig(configFilePath.c_str(), options);
  for (size_t i = 0; i < queries.size(); ++i) {
    if (queries[i].isDynamicVariable) {
      std::string dynKey =
          "Dynamic_" + std::to_string(hasher(queries[i].query));
      dynamicVars[i] = dynKey;
      pConfig->addConfigValue(dynKey.c_str(), (Hyprlang::STRING) "");
      if (debugLogging)
        spdlog::debug(std::string("[variable-search] Mapping query '") +
                      queries[i].query + "' to injected key '" + dynKey + "'");
    } else {
      dynamicVars[i] = queries[i].query;
      pConfig->addConfigValue(queries[i].query.c_str(), (Hyprlang::STRING) "");
    }
  }
  pConfig->commence();
}

std::vector<QueryResult>
executeQueries(const std::vector<hyprquery::QueryInput> &queries,
               const std::vector<std::string> &dynamicVars, bool debugLogging) {
  std::vector<QueryResult> results;
  for (size_t i = 0; i < queries.size(); ++i) {
    QueryResult result;
    result.key = queries[i].query;
    std::string lookupKey =
        queries[i].isDynamicVariable ? dynamicVars[i] : queries[i].query;
    if (debugLogging)
      spdlog::debug(std::string("[variable-search] Lookup key for query '") +
                    queries[i].query + "' is '" + lookupKey + "'");
    if (debugLogging && queries[i].isDynamicVariable) {
      spdlog::debug(std::string("[variable-search] Query '") +
                    queries[i].query + "' lookup key: '" + lookupKey + "'");
    }
    std::any value = pConfig->getConfigValue(lookupKey.c_str());
    result.value = hyprquery::ConfigUtils::convertValueToString(value);
    result.type = hyprquery::ConfigUtils::getValueTypeName(value);
    if (queries[i].isDynamicVariable && result.value == queries[i].query) {
      result.value = "";
      result.type = "NULL";
    }
    if (!queries[i].expectedType.empty()) {
      if (hyprquery::normalizeType(result.type) !=
          hyprquery::normalizeType(queries[i].expectedType)) {
        result.value = "";
        result.type = "NULL";
      }
    }
    if (!queries[i].expectedRegex.empty()) {
      try {
        std::regex rx(queries[i].expectedRegex);
        if (!std::regex_match(result.value, rx)) {
          result.value = "";
          result.type = "NULL";
        }
      } catch (const std::regex_error &) {
        result.value = "";
        result.type = "NULL";
      }
    }
    results.push_back(result);
  }
  return results;
}

void outputResults(const std::vector<QueryResult> &results, bool jsonOutput,
                   const std::string &delimiter) {
  if (jsonOutput) {
    nlohmann::json jsonArr = nlohmann::json::array();
    for (const auto &result : results) {
      jsonArr.push_back({{"key", result.key},
                         {"val", result.value},
                         {"type", result.type},
                         {"flags", result.flags}});
    }
    std::cout << jsonArr.dump(2) << std::endl;
  } else {
    for (size_t i = 0; i < results.size(); ++i) {
      std::cout << (results[i].type == "NULL" ? "" : results[i].value);
      if (i + 1 < results.size())
        std::cout << delimiter;
    }
    std::cout << std::endl;
  }
}

int main(int argc, char **argv) {
  CLI::App app{"hyprquery - A configuration parser for hypr* config files"};
  std::vector<std::string> rawQueries;
  std::string configFilePath;
  std::string schemaFilePath;
  bool allowMissing = false;
  bool getDefaultKeys = false;
  bool strictMode = false;
  bool jsonOutput = false;
  bool followSource = false;
  bool debugLogging = false;
  std::string delimiter = "\n";
  app.add_option(
         "--query,-Q", rawQueries,
         "Query to execute (format: query[expectedType][expectedRegex], can be "
         "specified multiple times)")
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
  CLI11_PARSE(app, argc, argv);
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
  std::vector<hyprquery::QueryInput> queries =
      hyprquery::parseQueryInputs(rawQueries);
  std::vector<std::string> dynamicVars;
  prepareConfig(queries, configFilePath, options, dynamicVars, debugLogging);
  if (!schemaFilePath.empty()) {
    hyprquery::ConfigUtils::addConfigValuesFromSchema(*pConfig, schemaFilePath);
  }
  if (followSource) {
    if (debugLogging)
      spdlog::debug("Registering source handler");
    hyprquery::SourceHandler::registerHandler(pConfig);
  }
  if (debugLogging) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::off);
  }
  const auto PARSERESULT = pConfig->parse();
  if (PARSERESULT.error) {
    if (debugLogging)
      spdlog::debug(std::string("Parse error: ") + PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }
  std::vector<QueryResult> results =
      executeQueries(queries, dynamicVars, debugLogging);
  int nullCount = 0;
  for (const auto &r : results) {
    if (r.type == "NULL")
      nullCount++;
  }
  outputResults(results, jsonOutput, delimiter);
  return nullCount > 0 ? 1 : 0;
}
