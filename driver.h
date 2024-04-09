#pragma once
#include <array>
#include <vector>
#include "inputs.h"

const int SIZE = 65536;
const std::string config_file = "ble_input_config.json";
int run_driver(std::array<char, SIZE> &shm, std::vector<Input>& inputs);
pid_t run_server();