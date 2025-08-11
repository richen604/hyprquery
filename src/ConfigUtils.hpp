#pragma once

#include <any>
#include <hyprlang.hpp>
#include <optional>
#include <string>
#include <vector>

namespace hyprquery {

struct QueryInput {
  std::string query;
  std::string expectedType;
  std::string expectedRegex;
  size_t index;
  bool isDynamicVariable = false;
};

struct QueryResult {
  std::string key;
  std::string value;
  std::string type;
  std::vector<std::string> flags;
};

std::string normalizeType(const std::string &type);

std::vector<QueryInput>
parseQueryInputs(const std::vector<std::string> &rawQueries);

class ConfigUtils {
public:
  static void addConfigValuesFromSchema(Hyprlang::CConfig &config,
                                        const std::string &schemaFilePath);

  static std::string convertValueToString(const std::any &value);
  static std::string getValueTypeName(const std::any &value);

  static std::optional<int64_t> configStringToInt(const std::string &str);

  static std::pair<int64_t, std::string>
  getWorkspaceIDNameFromString(const std::string &str);

  static std::optional<std::string>
  cleanCmdForWorkspace(const std::string &name, const std::string &cmd);

  static std::string normalizePath(const std::string &path);
};

} // namespace hyprquery