#pragma once
#include "ConfigUtils.hpp"
#include <vector>

namespace hyprquery {
void exportJson(const std::vector<QueryResult> &results);
}
