#pragma once
#include <vector>
#include <cstddef>

typedef struct {
    unsigned int minLen;
    unsigned int maxLen;
    std::vector<std::vector<std::byte>> validChoices;
} Field;

typedef struct {
    Field format;
    std::vector<std::byte> data;
    unsigned int energy;
} Input;