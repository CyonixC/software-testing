#pragma once
#include <vector>
#include <cstddef>
#include "json.hpp"

using json = nlohmann::json;

enum class FieldTypes {
    STRING,
    INTEGER,
    BINARY
};


typedef struct {
    unsigned int minLen;
    unsigned int maxLen;
    FieldTypes type;
    std::string name;
    std::vector<std::vector<std::byte>> validChoices;
} Field;

typedef struct {
    Field format;
    std::vector<std::byte> data;
} InputField;

typedef struct {
    std::vector<InputField> inputs;
    unsigned int energy;
    json to_json() const;
} InputSeed;

typedef struct {
    std::vector<std::byte> data;
    std::string name;
} Input;