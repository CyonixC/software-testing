#pragma once
#include <vector>
#include <cstddef>

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
} InputSeed;
