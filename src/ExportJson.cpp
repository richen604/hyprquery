#include "ConfigUtils.hpp"
#include <iostream>
#include <nlohmann/json.hpp>

#include <vector>

namespace hyprquery {

void exportJson(const std::vector<QueryResult> &results) {
  nlohmann::json jsonArr = nlohmann::json::array();
  for (const auto &result : results) {
    jsonArr.push_back({{"key", result.key},
                       {"val", result.value},
                       {"type", result.type},
                       {"flags", result.flags}});
  }
  std::cout << jsonArr.dump(2) << std::endl;
}

} // namespace hyprquery
