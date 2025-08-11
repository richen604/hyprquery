#include "ConfigUtils.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace hyprquery {

std::string envTransformKey(const std::string &key, bool isDynamic) {
  std::string out = key;

  std::replace(out.begin(), out.end(), ':', '_');
  std::replace(out.begin(), out.end(), '-', '_');

  if (isDynamic) {
    out = "__" + out;
  } else {
    out = "_" + out;
  }
  return out;
}

void exportEnv(const std::vector<QueryResult> &results,
               const std::vector<hyprquery::QueryInput> &queries) {
  for (size_t i = 0; i < results.size(); ++i) {
    const auto &result = results[i];
    const auto &query = queries[i];
    std::string envKey = envTransformKey(query.query, query.isDynamicVariable);
    std::cout << envKey << "=\"" << result.value << "\"\n";
  }
}

} // namespace hyprquery
