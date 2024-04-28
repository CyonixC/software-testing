#include <stdio.h>
#include <unistd.h>  // sleep
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>  // ifstream
#include <iostream>

#include "bug_checking.h"
#include "config.h"
#include "inputs.h"

int main(int argc, char const* argv[]) {
    std::filesystem::path currentDir = std::filesystem::current_path();

    // Convert path to string and print
    std::cout << "Current Directory: " << currentDir.string() << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_json_file>" << std::endl;
        return 1;
    }

    std::vector<Input> inputs_vector = inputsFromBugFile(argv[1], config_file);

    run_server();
    sleep(1);
    run_driver(inputs_vector);
    return 0;
}
