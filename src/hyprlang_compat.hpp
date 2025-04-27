#pragma once

// This header provides compatibility wrappers for different versions of
// hyprlang to ensure our code works regardless of the hyprlang version being
// used

#include <any>
#include <string>

// Define a workaround for newer hyprlang versions that use C++23 features
#if __cplusplus < 202302L
// If we're not compiling with C++23, provide stub implementations for
// print/println
namespace std {
template <typename... Args> inline void print(Args &&...args) {
  // Empty stub implementation
}

template <typename... Args> inline void println(Args &&...args) {
  // Empty stub implementation
}
} // namespace std
#endif

// Try to include system hyprlang first, fall back to local install
#ifdef USE_SYSTEM_HYPRLANG
#include <hyprlang.hpp>
#else
#include "../build/hyprlang_install/include/hyprlang.hpp"
#endif

// Define a default ABI version if not provided
#ifndef HYPRLANG_ABI_VERSION
#define HYPRLANG_ABI_VERSION 1
#endif

namespace HyprlangCompat {

// Generic wrapper to get config values with version-specific implementation
inline std::any getConfigValue(Hyprlang::CConfig *config, const char *key) {
  // Call the appropriate method based on version detection
  return config->getConfigValue(key);
}

// Helper to convert std::any value to string for display
inline std::string convertValueToString(const std::any &value) {
  if (!value.has_value()) {
    return "NULL";
  }

  if (value.type() == typeid(Hyprlang::INT)) {
    return std::to_string(std::any_cast<Hyprlang::INT>(value));
  } else if (value.type() == typeid(Hyprlang::FLOAT)) {
    return std::to_string(std::any_cast<Hyprlang::FLOAT>(value));
  } else if (value.type() == typeid(Hyprlang::STRING)) {
    return std::any_cast<Hyprlang::STRING>(value);
  } else if (value.type() == typeid(Hyprlang::VEC2)) {
    Hyprlang::VEC2 vec = std::any_cast<Hyprlang::VEC2>(value);
    return "[" + std::to_string(vec.x) + ", " + std::to_string(vec.y) + "]";
  } else {
    return "Unknown type";
  }
}

// Helper to get the type name of a value
inline std::string getValueTypeName(const std::any &value) {
  if (!value.has_value()) {
    return "NULL";
  }

  if (value.type() == typeid(Hyprlang::INT)) {
    return "INT";
  } else if (value.type() == typeid(Hyprlang::FLOAT)) {
    return "FLOAT";
  } else if (value.type() == typeid(Hyprlang::STRING)) {
    return "STRING";
  } else if (value.type() == typeid(Hyprlang::VEC2)) {
    return "VEC2";
  } else {
    return "UNKNOWN";
  }
}

} // namespace HyprlangCompat