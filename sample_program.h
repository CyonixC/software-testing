#pragma once
#include <array>
#include "inputs.h"

const int SIZE = 65536;
pid_t run_server();
int run_driver(std::array<char, SIZE> &shm, InputSeed input);
int hash_cov_into_shm(std::array<char, SIZE>& shm, const char* filename);
int run_coverage_shm(std::array<char, SIZE>& shm, char a, char b);