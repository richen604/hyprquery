#include "ConfigUtils.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <spdlog/spdlog.h>
#include <wordexp.h>

namespace hyprquery {

void ConfigUtils::addConfigValuesFromSchema(Hyprlang::CConfig &config,
                                            const std::string &schemaFilePath) {
  std::ifstream schemaFile(schemaFilePath);
  if (!schemaFile.is_open()) {
    spdlog::error("Failed to open schema file: {}", schemaFilePath);
    return;
  }

  nlohmann::json schemaJson;
  try {
    schemaFile >> schemaJson;
  } catch (const std::exception &e) {
    spdlog::error("Failed to parse schema JSON: {}", e.what());
    return;
  }

  if (!schemaJson.contains("hyprlang_schema")) {
    spdlog::error("Invalid schema format: missing 'hyprlang_schema' key");
    return;
  }

  for (const auto &option : schemaJson["hyprlang_schema"]) {
    if (!option.contains("value") || !option.contains("type") ||
        !option.contains("data")) {
      spdlog::error("Invalid schema option format");
      continue;
    }

    std::string value = option["value"].get<std::string>();
    std::string type = option["type"].get<std::string>();

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

std::string ConfigUtils::convertValueToString(const std::any &value) {
  if (value.type() == typeid(Hyprlang::INT)) {
    return std::to_string(std::any_cast<Hyprlang::INT>(value));
  } else if (value.type() == typeid(Hyprlang::FLOAT)) {
    return std::to_string(std::any_cast<Hyprlang::FLOAT>(value));
  } else if (value.type() == typeid(Hyprlang::STRING)) {
    return std::any_cast<Hyprlang::STRING>(value);
  } else if (value.type() == typeid(Hyprlang::VEC2)) {
    const auto &vec = std::any_cast<Hyprlang::VEC2>(value);
    return std::to_string(vec.x) + ", " + std::to_string(vec.y);
  } else if (value.has_value()) {
    return "non-standard value";
  } else {
    return "UNSET";
  }
}

std::string ConfigUtils::getValueTypeName(const std::any &value) {
  if (value.type() == typeid(Hyprlang::INT)) {
    return "INT";
  } else if (value.type() == typeid(Hyprlang::FLOAT)) {
    return "FLOAT";
  } else if (value.type() == typeid(Hyprlang::STRING)) {
    return "STRING";
  } else if (value.type() == typeid(Hyprlang::VEC2)) {
    return "VEC2";
  } else if (!value.has_value()) {
    return "NULL";
  } else {
    return "CUSTOM";
  }
}

std::optional<int64_t> ConfigUtils::configStringToInt(const std::string &str) {
  if (str == "true" || str == "on" || str == "yes")
    return 1;
  if (str == "false" || str == "off" || str == "no")
    return 0;

  try {
    return std::stoll(str);
  } catch (...) {
    return std::nullopt;
  }
}

std::pair<int64_t, std::string>
ConfigUtils::getWorkspaceIDNameFromString(const std::string &str) {
  static const int64_t WORKSPACE_INVALID = -99;

  if (str.starts_with("name:")) {
    return {WORKSPACE_INVALID, str.substr(5)};
  }

  try {
    return {std::stoll(str), ""};
  } catch (...) {
    return {WORKSPACE_INVALID, ""};
  }
}

std::optional<std::string>
ConfigUtils::cleanCmdForWorkspace(const std::string &name,
                                  const std::string &cmd) {
  if (cmd.empty())
    return std::nullopt;

  std::string result = cmd;

  std::regex namePattern("\\$NAME");
  result = std::regex_replace(result, namePattern, name);

  return result;
}

std::string ConfigUtils::normalizePath(const std::string &path) {

  std::string processedPath = path;
  if ((processedPath.front() == '"' && processedPath.back() == '"') ||
      (processedPath.front() == '\'' && processedPath.back() == '\'')) {
    processedPath = processedPath.substr(1, processedPath.length() - 2);
  }

  std::string expandedPath = processedPath;

  if (expandedPath.find('$') != std::string::npos) {
    wordexp_t p;

    if (wordexp(expandedPath.c_str(), &p, WRDE_NOCMD) == 0) {
      if (p.we_wordc > 0 && p.we_wordv[0] != nullptr) {
        expandedPath = p.we_wordv[0];
      }
      wordfree(&p);
    }
  }

  if (expandedPath.starts_with("~") &&
      (expandedPath.size() == 1 || expandedPath[1] == '/')) {
    const char *home = getenv("HOME");
    if (home) {
      expandedPath.replace(0, 1, home);
    }
  }

  std::filesystem::path fsPath(expandedPath);

  if (fsPath.is_relative()) {
    fsPath = std::filesystem::absolute(fsPath);
  }

  if (std::filesystem::exists(fsPath)) {
    return std::filesystem::canonical(fsPath).string();
  }

  if (std::filesystem::exists(fsPath.parent_path())) {
    return std::filesystem::weakly_canonical(fsPath).string();
  }

  return fsPath.lexically_normal().string();
}

} // namespace hyprquery