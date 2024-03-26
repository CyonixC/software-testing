#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>  // ifstream
#include <iostream>
#include <queue>
#include <random>
#include <stdio.h>
#include <sys/wait.h> // For waitpid()
#include <unistd.h> // For fork(), execvp()

#include "config.h"
#include "inputs.h"
#include "sample_program.h"

const std::string config_file = "input_config_example.json";
namespace fs = std::filesystem;
const fs::path output_directory{"fuzz_out"};

template<typename T>
T random_int(T min, T max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

int run_driver(std::array<char, SIZE> &shm, InputSeed input);
InputSeed mutateSeed(InputSeed seed);
bool isInteresting(std::array<char, SIZE> data);

int main() {
    // Initialise the coverage measurement buffer
    std::array<char, SIZE> coverage_arr{};

    // Initialise the seed queue
    std::queue<InputSeed> seedQueue;

    // Read the config file
    std::ifstream file{config_file};
    const json config = json::parse(file);
    std::vector<Field> fields = readFields(config);
    if (!config.contains("seed_folder")) {
        throw std::runtime_error("Config file does not contain a seed folder path");
    }
    const fs::path seed_folder{config["seed_folder"]};

    // Create output folder
    fs::create_directories(output_directory / "interesting");
    fs::create_directories(output_directory / "crash");

    // Read the seed file
    for (auto const& seed_file : fs::directory_iterator{seed_folder}) {
        std::ifstream seed{seed_file.path()};
        json seed_json = json::parse(seed);
        InputSeed seed_input = readSeed(seed_json, fields);

        seedQueue.push(seed_input);
    }
    
    // Run the coverage Python script to generate the .coverage file
    // This is a stand-in for the actual server, and is just listening for connections on 4345.
    pid_t pid = run_server();
    printf("Server started\n");
    sleep(1); // Wait for the server to start, on actual should probably use a signal or something

    unsigned int interesting_count = 0;
    unsigned int crash_count = 0;

    while (true) {
        InputSeed i = seedQueue.front();
        seedQueue.pop();

        for (int j = 0; j < i.energy; j++) {
            InputSeed mutated = mutateSeed(i);
        
            // Run the driver a few times
            bool failed = run_driver(coverage_arr, mutated);
            if (isInteresting(coverage_arr)) {
                seedQueue.emplace(mutated);
                std::cout << "Interesting: " << mutated.to_json() << std::endl;
                
                // Output interesting input as a file in the output directory
                std::ostringstream filename;
                fs::path output_path;
                if (!failed) {
                    filename << "input" << interesting_count << ".json";
                    output_path = output_directory / "interesting" / filename.str();
                    interesting_count++;
                }
                else {
                    filename << "input" << crash_count << ".json";
                    output_path = output_directory / "crash" / filename.str();
                    crash_count++;
                }
                std::ofstream output_file{output_path};
                output_file << std::setw(4) << mutated.to_json() << std::endl;
            }

            // Zero out the coverage array
            for (auto &elem : coverage_arr) {
                elem = 0;
            }

        }
        seedQueue.emplace(i);

    }
    kill(pid, SIGTERM); // Kill the Python server
}



bool isInteresting(std::array<char, SIZE> data) {
    // This is a mirror of the array produced by the coverage tool
    // to track which branches have been taken

    // Each char element contains the bucket count of the number of times
    // a branch has been taken.
    static char* tracking = new char[SIZE]();

    // Bucketing branch transition counts
    bool is_interesting = false;
    for (int i = 0; i < SIZE; i++) {
        if (data[i] >= 128 && tracking[i] >> 7 == 0) {
            tracking[i] |= 0b10000000;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] >= 32 && (tracking[i] >> 6) % 2 == 0) {
            tracking[i] |= 0b01000000;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] >= 16 && (tracking[i] >> 5) % 2 == 0) {
            tracking[i] |= 0b00100000;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] >= 8 && (tracking[i] >> 4) % 2 == 0) {
            tracking[i] |= 0b00010000;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] >= 4 && (tracking[i] >> 3) % 2 == 0) {
            tracking[i] |= 0b00001000;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] == 3 && (tracking[i] >> 2) % 2 == 0) {
            tracking[i] |= 0b00000100;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] == 2 && (tracking[i] >> 1) % 2 == 0) {
            tracking[i] |= 0b00000010;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        } else if (data[i] == 1 && tracking[i] % 2 == 0) {
            tracking[i] |= 0b00000001;
            // std::cout << "new path: " << i << std::endl;
            is_interesting = true;
        }
    }

    return is_interesting;
}

void assignEnergy(InputSeed& input) {
    // Just assign constant energy
    const int ENERGY = 100;
    input.energy = ENERGY;
}

void mutate(InputField& input) {
    if (input.data.empty()) return; // Check if there's data to mutate

    // Decide how many bytes to mutate based on the size of the data
    size_t mutationsCount = random_int<size_t>(1, input.data.size() / 2);

    for (size_t i = 0; i < mutationsCount; ++i) {
        // Pick a random byte to mutate
        size_t index = random_int<size_t>(0, input.data.size() - 1);

        // Generate a new random byte and replace the chosen byte
        std::byte newValue = static_cast<std::byte>(random_int<int>(0, 255));
        input.data[index] = newValue;
    }

}

InputSeed mutateSeed(InputSeed seed) {
    // Copies
    for (auto &elem : seed.inputs) {
        mutate(elem);
    }
    return seed;
}