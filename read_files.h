#pragma once

#include "json.hpp"
#include "inputs.h"

using json = nlohmann::json;

std::vector<Field> readFields(const json& j);
std::vector<Input> readSeed(const json &j, std::vector<Field> &fields);
