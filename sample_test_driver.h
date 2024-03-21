#pragma once
#include <array>

const int SIZE = 65536;
int hash_cov_into_shm(std::array<char, SIZE>& shm, const char* filename);
int run_coverage_shm(std::array<char, SIZE>& shm, char a, char b);