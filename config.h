#pragma once
#include <vector>
#include "json.hpp"
#include "inputs.h"

using json = nlohmann::json;

std::vector<Field> readFields(const json& j);
InputSeed readSeed(const json &j, std::vector<Field> &fields);
std::vector<std::byte> int_to_binary(const std::vector<uint8_t> &json_bin);
std::vector<uint8_t> binary_to_int(const std::vector<std::byte> &byte_arr);