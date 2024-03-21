#pragma once

int hash_cov_into_shm(char* shm, const char* filename);
int run_coverage_shm(std::array<char, SIZE> shm);
const int SIZE = 65536;