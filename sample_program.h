#pragma once
#include <array>
#include "inputs.h"
#include "config.h"
#include "driver.h"

int hash_cov_into_shm(std::array<char, SIZE>& shm, const char* filename);
int run_coverage_shm(std::array<char, SIZE>& shm, char a, char b);