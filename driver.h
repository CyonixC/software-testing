#pragma once
#include <array>
#include <vector>
#include "inputs.h"

const int SIZE = 65536;
int run_driver(std::array<char, SIZE> &shm, std::vector<Input>& inputs);
pid_t run_server();