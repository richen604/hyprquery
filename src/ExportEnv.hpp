#pragma once
#include "ConfigUtils.hpp"

#include <vector>

namespace hyprquery {
void exportEnv(const std::vector<QueryResult> &results,
               const std::vector<QueryInput> &queries);
}
