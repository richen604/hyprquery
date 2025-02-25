#include "../build/_deps/nlohmann_json-src/single_include/nlohmann/json.hpp"
#include <CLI/CLI.hpp>
#include <any>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

#include "../build/hyprlang_install/include/hyprlang.hpp"

static Hyprlang::CConfig *pConfig = nullptr;
static std::string currentPath = "";

std::string expandEnvVars(const std::string &path) {
  std::string result;
  result.reserve(path.size());

  for (size_t i = 0; i < path.size(); ++i) {
    if (path[i] == '$') {
      size_t j = i + 1;
      while (j < path.size() && (std::isalnum(path[j]) || path[j] == '_')) {
        ++j;
      }
      std::string var = path.substr(i + 1, j - i - 1);
      const char *value = std::getenv(var.c_str());
      if (value) {
        result += value;
      }
      i = j - 1;
    } else {
      result += path[i];
    }
  }

  return result;
}

std::filesystem::path resolvePath(const std::string &path) {
  std::string expandedPath = expandEnvVars(path);
  std::filesystem::path fsPath(expandedPath);
  if (fsPath.is_relative()) {
    return std::filesystem::canonical(std::filesystem::current_path() / fsPath);
  }
  return fsPath;
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

  for (const auto &option : schemaJson["config_options"]) {
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

static Hyprlang::CParseResult handleSource(const char *COMMAND,
                                           const char *VALUE) {
  std::string PATH = resolvePath(currentPath + "/" + VALUE).string();
  return pConfig->parseFile(PATH.c_str());
}

struct QueryResult {
  std::string key;
  std::string value;
  std::string type;
  std::vector<std::string> flags;
};

void printQueryResultAsJson(const QueryResult &result) {
  nlohmann::json jsonResult;
  jsonResult["key"] = result.key;
  jsonResult["val"] = result.value;
  jsonResult["type"] = result.type;
  jsonResult["flags"] = result.flags;

  std::cout << jsonResult.dump(2) << std::endl;
}

void printQueryResultAsPlainText(const QueryResult &result) {
  std::cout << result.value << std::endl;
}

int main(int argc, char **argv, char **envp) {
  CLI::App app{"hyprparser - A configuration parser for hypr* config files"};

  std::string query;
  std::string configFilePath;
  std::string schemaFilePath;
  bool allowMissing = false;
  bool getDefaultKeys = false;
  bool strictMode = false;
  bool jsonOutput = false;

  app.add_option("--query", query, "Query to execute")->required();
  app.add_option("config_file", configFilePath, "Configuration file")
      ->required();
  app.add_option("--schema", schemaFilePath, "Schema file");
  app.add_flag("--allow-missing", allowMissing, "Allow missing values");
  app.add_flag("--get-defaults", getDefaultKeys, "Get default keys");
  app.add_flag("--strict", strictMode, "Enable strict mode");
  app.add_flag("--json,-j", jsonOutput, "Output result in JSON format");

  CLI11_PARSE(app, argc, argv);

  configFilePath = resolvePath(configFilePath).string();
  if (!schemaFilePath.empty()) {
    schemaFilePath = resolvePath(schemaFilePath).string();
  }

  Hyprlang::SConfigOptions options;
  options = {.verifyOnly = getDefaultKeys ? 1 : 0, .allowMissingConfig = 1};
  Hyprlang::CConfig config(configFilePath.c_str(), options);
  pConfig = &config;
  currentPath = std::filesystem::canonical(configFilePath).parent_path();

  if (!schemaFilePath.empty()) {
    addConfigValuesFromSchema(config, schemaFilePath);
  }

  config.addConfigValue(query.c_str(), (Hyprlang::STRING) "dynamic");
  config.registerHandler(&handleSource, "source", {.allowFlags = false});

  config.commence();

  const auto PARSERESULT = config.parse();
  if (PARSERESULT.error) {
    spdlog::debug("Parse error: {}", PARSERESULT.getError());
    if (strictMode) {
      return 1;
    }
  }

  std::any value = config.getConfigValue(query.c_str());
  QueryResult result;
  result.key = query;

  if (value.has_value()) {
    if (value.type() == typeid(Hyprlang::INT)) {
      result.value = std::to_string(std::any_cast<Hyprlang::INT>(value));
      result.type = "INT";
    } else if (value.type() == typeid(Hyprlang::FLOAT)) {
      result.value = std::to_string(std::any_cast<Hyprlang::FLOAT>(value));
      result.type = "FLOAT";
    } else if (value.type() == typeid(Hyprlang::STRING)) {
      result.value = std::any_cast<Hyprlang::STRING>(value);
      result.type = "STRING";
    } else if (value.type() == typeid(Hyprlang::VEC2)) {
      Hyprlang::VEC2 vec = std::any_cast<Hyprlang::VEC2>(value);
      result.value =
          "[" + std::to_string(vec.x) + ", " + std::to_string(vec.y) + "]";
      result.type = "VEC2";
    } else {
      result.value = "Unknown";
      result.type = "UNKNOWN";
    }
  } else {
    result.value = "NULL";
    result.type = "NULL";
  }

  if (jsonOutput) {
    printQueryResultAsJson(result);
  } else {
    printQueryResultAsPlainText(result);
  }

  return 0;
}
