#pragma once
#include <array>
#include <vector>
#include "inputs.h"

#define STRINGIFY(x) #x
#define GETENV(x) STRINGIFY(x)

const int SIZE = 65536;
const std::string config_file = "./configs/django.json";
int run_driver(std::array<char, SIZE>& shm, std::vector<Input>& inputs);
pid_t run_server();