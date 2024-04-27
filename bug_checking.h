#pragma once
#include <array>
#include <vector>
#include "inputs.h"

#define STRINGIFY(x) #x
#define GETENV(x) STRINGIFY(x)

const std::string config_file = GETENV(CONFIG_FILE);
int run_driver(std::vector<Input>& inputs);
pid_t run_server();