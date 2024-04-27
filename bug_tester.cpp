#include <stdio.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>  // ifstream
#include <iostream>
#include <unistd.h> // sleep

#include "config.h"
#include "bug_checking.h"
#include "inputs.h"

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json_file>" << std::endl;
        return 1;
    }
    
    std::vector<Input> inputs_vector = inputsFromBugFile(argv[1]);

    run_server();
    sleep(1);
    run_driver(inputs_vector);
    return 0;
}
