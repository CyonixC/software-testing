#pragma once
#include <cstddef>
#include <vector>
#include "json.hpp"

using json = nlohmann::json;

enum class FieldTypes { STRING, INTEGER, BINARY };

typedef struct {
    unsigned int minLen;
    unsigned int maxLen;
    FieldTypes type;
    std::string name;
    std::vector<std::vector<std::byte>> validChoices;
    std::vector<std::byte> validSet;
} Field;

typedef struct {
    Field format;
    std::vector<std::byte> data;
} InputField;

typedef struct {
    std::vector<InputField> inputs;
    unsigned int energy;
    json to_json() const;
    int chosen_count = 0;
} InputSeed;

typedef struct {
    std::vector<std::byte> data;
    std::string name;
} Input;

json inputVectorToJSON(const std::vector<Input>& input);
std::vector<Input> makeInputsFromSeed(const InputSeed& seed);