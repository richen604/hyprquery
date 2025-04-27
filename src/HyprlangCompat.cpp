#include "HyprlangCompat.hpp"
#include <spdlog/spdlog.h>

namespace hyprquery {

std::any HyprlangCompat::getConfigValue(Hyprlang::CConfig *config,
                                        const std::string &name) {
  if (!config) {
    spdlog::error("Null config pointer in getConfigValue");
    return {};
  }

  auto valuePtr = config->getConfigValuePtr(name.c_str());
  if (!valuePtr) {
    spdlog::debug("Config value not found: {}", name);
    return {};
  }

  return valuePtr->getValue();
}

std::string HyprlangCompat::convertValueToString(const std::any &value) {
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

std::string HyprlangCompat::getValueTypeName(const std::any &value) {
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

} // namespace hyprquery