#include <stdio.h>
#include <sys/wait.h>  // For waitpid()
#include <unistd.h>    // For fork(), execvp()
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>  // ifstream
#include <iostream>
#include <queue>
#include <random>

#include "config.h"
#include "driver.h"
#include "inputs.h"
#include "sample_program.h"

#define STRINGIFY(x) #x
#define GETENV(x) STRINGIFY(x)

namespace fs = std::filesystem;
const std::string program_name = GETENV(PROGRAM_NAME);
const std::string output_dir = program_name + "_out";
const fs::path output_directory{output_dir};

static int8_t interesting_8[] = {INTERESTING_8};
static int16_t interesting_16[] = {INTERESTING_8, INTERESTING_16};
static int32_t interesting_32[] = {INTERESTING_8, INTERESTING_16,
                                   INTERESTING_32};
void fuzz(std::vector<std::byte>& fuzz_data, int minLen, int maxLen);
void fuzz_set(std::vector<std::byte>& fuzz_data,
              const std::vector<std::byte>& valid_set, int minLen, int maxLen);

// Change endianness of a 16 bit value
uint16_t SWAP16(uint16_t _x) {
    uint16_t _ret = (_x);
    return (uint16_t)((_ret << 8) | (_ret >> 8));
}

// Change endianness of 32 bit value
uint32_t SWAP32(uint32_t _x) {
    uint32_t _ret = (_x);
    return (uint32_t)((_ret << 24) | (_ret >> 24) | ((_ret << 8) & 0x00FF0000) |
                      ((_ret >> 8) & 0x0000FF00));
}

template <typename T>
T random_int(T min, T max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

std::vector<Input> makeInputsFromSeed(const InputSeed& seed);
InputSeed mutateSeed(InputSeed seed);
bool isInteresting(std::array<char, SIZE>& data, bool failed);
void assignEnergy(InputSeed& input, int seed_count);

uint32_t rand32(uint32_t limit) {
    if (limit <= 1)
        return 0;
    return random_int<uint32_t>(0, limit - 1);
}

void FLIP_BIT(std::byte* data_array, uint32_t bit_index) {
    char* data_arr = (char*)data_array;
    data_arr[(bit_index) >> 3] ^= (128 >> ((bit_index) & 7));
}

/* Helper to choose random block len for block operations in fuzz_one().
   Doesn't return zero, provided that max_len is > 0. */

static uint32_t choose_block_len(uint32_t limit) {

    uint32_t min_value, max_value;
    // uint32_t rlim = MIN(queue_cycle, 3);

    switch (rand32(3)) {

        case 0:
            min_value = 1;
            max_value = HAVOC_BLK_SMALL;
            break;

        case 1:
            min_value = HAVOC_BLK_SMALL;
            max_value = HAVOC_BLK_MEDIUM;
            break;

        default:

            if (rand32(10)) {

                min_value = HAVOC_BLK_MEDIUM;
                max_value = HAVOC_BLK_LARGE;

            } else {

                min_value = HAVOC_BLK_LARGE;
                max_value = HAVOC_BLK_XL;
            }
    }

    if (min_value >= limit)
        min_value = 1;

    max_value = max_value < limit ? max_value : limit;

    return min_value + rand32(max_value - min_value + 1);
}

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
        throw std::runtime_error(
            "Config file does not contain a seed folder path");
    }
    const fs::path seed_folder{config["seed_folder"]};

    // Create output folder
    fs::create_directories(output_directory / "interesting");
    fs::create_directories(output_directory / "crash");

    // Create time file and clear its contents
    std::ofstream tfile{output_directory / "time", std::ios::trunc};
    std::ofstream efile{output_directory / "effi", std::ios::trunc};
    tfile.close();
    efile.close();

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
    sleep(
        5);  // Wait for the server to start, on actual should probably use a signal or something

    unsigned int interesting_count = 0;
    unsigned int crash_count = 0;
    auto startTime = std::chrono::system_clock::now();
    auto startMillisecondsSinceEpoch =
        std::chrono::time_point_cast<std::chrono::milliseconds>(startTime)
            .time_since_epoch()
            .count();

    while (true) {
        InputSeed i = seedQueue.front();
        assignEnergy(i, seedQueue.size());
        seedQueue.pop();

        auto seed_start_time =
            std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now())
                .time_since_epoch()
                .count();
        auto seed_interesting_count = 0;
        auto seed_crash_count = 0;
        int64_t mutation_time = 0;
        int64_t driver_time = 0;

        for (int j = 0; j < i.energy; j++) {
            auto mutation_start_time =
                std::chrono::time_point_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now())
                    .time_since_epoch()
                    .count();
            InputSeed mutated = mutateSeed(i);
            std::vector<Input> inputs = makeInputsFromSeed(mutated);

            auto mutation_end_time =
                std::chrono::time_point_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now())
                    .time_since_epoch()
                    .count();
            // Run the driver a few times
            bool failed = run_driver(coverage_arr, inputs);

            auto driver_end_time =
                std::chrono::time_point_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now())
                    .time_since_epoch()
                    .count();
            mutation_time += mutation_end_time - mutation_start_time;
            driver_time += driver_end_time - mutation_end_time;

            if (failed) {
                kill(pid, SIGTERM);
                pid = run_server();
                sleep(5);  // Wait for the server to start, on actual should probably use a signal or something
            }
            if (isInteresting(coverage_arr, failed)) {
                seedQueue.emplace(mutated);
                std::cout << "Interesting: " << mutated.to_json() << std::endl;

                // Output interesting input as a file in the output directory
                std::ostringstream filename;
                std::ofstream time_file{output_directory / "time",
                                        std::ios::app};
                fs::path output_path;
                auto currentTime = std::chrono::system_clock::now();
                auto millisecondsSinceEpoch =
                    std::chrono::time_point_cast<std::chrono::milliseconds>(
                        currentTime)
                        .time_since_epoch()
                        .count();
                auto timeSinceStart =
                    millisecondsSinceEpoch - startMillisecondsSinceEpoch;

                if (failed) {
                    seed_crash_count++;
                    filename << "input" << crash_count << ".json";
                    output_path = output_directory / "crash" / filename.str();
                    time_file << "C," << timeSinceStart << std::endl;
                    crash_count++;
                } else {
                    seed_interesting_count++;
                    filename << "input" << interesting_count << ".json";
                    output_path =
                        output_directory / "interesting" / filename.str();
                    time_file << "I," << timeSinceStart << std::endl;
                    interesting_count++;
                }
                std::ofstream output_file{output_path};
                output_file << std::setw(4) << mutated.to_json() << std::endl;
                output_file.close();
                time_file.close();
            }

            // Zero out the coverage array
            for (auto& elem : coverage_arr) {
                elem = 0;
            }

            // /* If we're finding new stuff, let's run for a bit longer, limits
            // permitting. */

            // if (queued_paths != havoc_queued) {

            //   if (perf_score <= HAVOC_MAX_MULT * 100) {
            //     stage_max  *= 2;
            //     perf_score *= 2;
            //   }

            //   havoc_queued = queued_paths;
        }

        auto seed_finish_time =
            std::chrono::time_point_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now())
                .time_since_epoch()
                .count();
        std::ofstream effi_file{output_directory / "effi", std::ios::app};
        effi_file << seed_finish_time - seed_start_time << "," << i.energy
                  << "," << seed_interesting_count << "," << seed_crash_count
                  << "," << mutation_time << "," << driver_time << std::endl;
        effi_file.close();

        seedQueue.emplace(i);
    }
    kill(pid, SIGTERM);  // Kill the Python server
}

bool isInteresting(std::array<char, SIZE>& data, bool failed) {
    // This is a mirror of the array produced by the coverage tool
    // to track which branches have been taken

    // Each char element contains the bucket count of the number of times
    // a branch has been taken.
    // Separate bucket for failures and succeeds
    static char* failed_tracking = new char[SIZE]();
    static char* good_tracking = new char[SIZE]();

    auto tracking = failed ? failed_tracking : good_tracking;

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

void assignEnergy(InputSeed& input, int seed_count) {
    // Metadata about number of times we chose a seed
    static int chosen_count_total = 0;
    const int BASE_ENERGY = 1;

    // If it's above average, don't fuzz it again
    if (input.chosen_count > chosen_count_total / seed_count) {
        input.energy = 0;
        return;
    }

    // If it's below, fuzz it with exponential growth
    input.chosen_count++;
    chosen_count_total++;
    input.energy = BASE_ENERGY * static_cast<int>(pow(2, input.chosen_count));
    return;
}

InputSeed mutateSeed(InputSeed seed) {
    seed.chosen_count = 0;
    seed.energy = 0;
    // Copies the input seed to avoid mangling it
    for (auto& elem : seed.inputs) {
        if (!elem.format.validChoices.empty()) {

            // if there are a set of valid choices, pick a random one to be the next input
            elem.data =
                elem.format
                    .validChoices[rand32(elem.format.validChoices.size())];

        } else if (elem.format.validSet.size() > 0) {
            // If there is a valid set, use it to mutate the input
            fuzz_set(elem.data, elem.format.validSet, elem.format.minLen,
                     elem.format.maxLen);
        } else {

            // Otherwise, put it through the mutation process.
            fuzz(elem.data, elem.format.minLen, elem.format.maxLen);
        }
    }
    return seed;
}

void fuzz_set(std::vector<std::byte>& fuzz_data,
              const std::vector<std::byte>& valid_set, int minLen, int maxLen) {
    int32_t stage_max = 1;  // arbitrary for now

    for (int32_t stage_cur = 0; stage_cur < stage_max; stage_cur++) {

        uint32_t use_stacking = 1 << (1 + rand32(HAVOC_STACK_POW2));

        // stage_cur_val = use_stacking;

        for (uint32_t i = 0; i < use_stacking; i++) {
            uint32_t c = rand32(5);
            switch (c) {
                case 0: {

                    /* Just set a random byte to a random value. Because,
                      why not. We use XOR with 1-255 to eliminate the
                      possibility of a no-op. */

                    uint32_t pos = rand32(fuzz_data.size());
                    fuzz_data[pos] = valid_set[rand32(valid_set.size())];
                    break;
                }
                case 1 ... 2: {

                    /* Delete bytes. We're making this a bit more likely
                    than insertion (the next option) in hopes of keeping
                    files reasonably small. */

                    uint32_t del_from, del_len;

                    if (fuzz_data.size() <= minLen)
                        break;

                    /* Don't delete too much. */

                    del_len = choose_block_len(fuzz_data.size() - minLen);

                    del_from = rand32(fuzz_data.size() - del_len + 1);

                    memmove(fuzz_data.data() + del_from,
                            fuzz_data.data() + del_from + del_len,
                            fuzz_data.size() - del_from - del_len);

                    break;
                }
                case 3: {

                    if (fuzz_data.size() + HAVOC_BLK_XL < MAX_FILE) {

                        /* Clone bytes (75%) or insert a block of constant bytes (25%). */

                        uint8_t actually_clone = rand32(4);
                        uint32_t clone_from, clone_to, clone_len;
                        std::vector<std::byte> new_buf = fuzz_data;
                        uint32_t max_blk_len = fuzz_data.size() < maxLen
                                                   ? fuzz_data.size()
                                                   : maxLen;

                        if (actually_clone) {
                            clone_len = choose_block_len(max_blk_len);
                            clone_from =
                                rand32(fuzz_data.size() - clone_len + 1);

                        } else {

                            clone_len = choose_block_len(max_blk_len);
                            clone_from = 0;
                        }

                        uint32_t clone_to_lim =
                            fuzz_data.size() < maxLen - clone_len
                                ? fuzz_data.size()
                                : maxLen - clone_len;
                        clone_to = rand32(clone_to_lim);

                        if (clone_to + clone_len > new_buf.size()) {
                            new_buf.resize(clone_to + clone_len);
                        }

                        /* Inserted part */
                        if (actually_clone)
                            memcpy(new_buf.data() + clone_to,
                                   fuzz_data.data() + clone_from, clone_len);
                        else
                            memset(
                                new_buf.data() + clone_to,
                                rand32(2)
                                    ? static_cast<char>(
                                          valid_set[rand32(valid_set.size())])
                                    : static_cast<char>(
                                          fuzz_data[rand32(fuzz_data.size())]),
                                clone_len);

                        if (new_buf.size() > maxLen) {
                            int a = 1;
                        }
                        fuzz_data = std::move(new_buf);
                    }

                    break;
                }

                case 4: {

                    /* Overwrite bytes with a randomly selected chunk (75%) or fixed
                      bytes (25%). */

                    uint32_t copy_from, copy_to, copy_len;

                    if (fuzz_data.size() < 2)
                        break;

                    copy_len = choose_block_len(fuzz_data.size() - 1);

                    copy_from = rand32(fuzz_data.size() - copy_len + 1);
                    copy_to = rand32(fuzz_data.size() - copy_len + 1);

                    if (rand32(4)) {

                        if (copy_from != copy_to)
                            memmove(fuzz_data.data() + copy_to,
                                    fuzz_data.data() + copy_from, copy_len);

                    } else
                        memset(fuzz_data.data() + copy_to,
                               rand32(2)
                                   ? static_cast<char>(
                                         valid_set[rand32(valid_set.size())])
                                   : static_cast<char>(
                                         fuzz_data[rand32(fuzz_data.size())]),
                               copy_len);

                    break;
                }
            }
        }
    }
}

void fuzz(std::vector<std::byte>& fuzz_data, int minLen, int maxLen) {

    int32_t stage_max = 1;  // arbitrary for now

    for (int32_t stage_cur = 0; stage_cur < stage_max; stage_cur++) {

        uint32_t use_stacking = 1 << (1 + rand32(HAVOC_STACK_POW2));

        // stage_cur_val = use_stacking;

        for (uint32_t i = 0; i < use_stacking; i++) {
            uint32_t c = rand32(15);
            switch (c) {

                case 0:

                    /* Flip a single bit somewhere. Spooky! */

                    FLIP_BIT(fuzz_data.data(), rand32(fuzz_data.size() << 3));
                    break;

                case 1:

                    /* Set byte to interesting value. */

                    fuzz_data[rand32(fuzz_data.size())] =
                        (std::byte)interesting_8[rand32(sizeof(interesting_8))];
                    break;

                case 2:

                    /* Set word to interesting value, randomly choosing endian. */

                    if (fuzz_data.size() < 2)
                        break;

                    if (rand32(2)) {

                        *(uint16_t*)(fuzz_data.data() +
                                     rand32(fuzz_data.size() - 1)) =
                            interesting_16[rand32(sizeof(interesting_16) >> 1)];

                    } else {

                        *(uint16_t*)(fuzz_data.data() +
                                     rand32(fuzz_data.size() - 1)) =
                            SWAP16(interesting_16[rand32(
                                sizeof(interesting_16) >> 1)]);
                    }

                    break;

                case 3:

                    /* Set dword to interesting value, randomly choosing endian. */

                    if (fuzz_data.size() < 4)
                        break;

                    if (rand32(2)) {

                        *(uint32_t*)(fuzz_data.data() +
                                     rand32(fuzz_data.size() - 3)) =
                            interesting_32[rand32(sizeof(interesting_32) >> 2)];

                    } else {

                        *(uint32_t*)(fuzz_data.data() +
                                     rand32(fuzz_data.size() - 3)) =
                            SWAP32(interesting_32[rand32(
                                sizeof(interesting_32) >> 2)]);
                    }

                    break;

                case 4: {

                    /* Randomly subtract from byte. */

                    uint32_t pos = rand32(fuzz_data.size());
                    fuzz_data[pos] = static_cast<std::byte>(
                        static_cast<char>(fuzz_data[pos]) -
                        static_cast<char>(1 + rand32(ARITH_MAX)));
                    break;
                }

                case 5: {

                    /* Randomly add to byte. */

                    uint32_t pos = rand32(fuzz_data.size());
                    fuzz_data[pos] = static_cast<std::byte>(
                        static_cast<char>(fuzz_data[pos]) +
                        static_cast<char>(1 + rand32(ARITH_MAX)));
                    break;
                }

                case 6: {

                    /* Randomly subtract from word, random endian. */

                    if (fuzz_data.size() < 2)
                        break;

                    if (rand32(2)) {

                        uint32_t pos = rand32(fuzz_data.size() - 1);

                        *(uint16_t*)(fuzz_data.data() + pos) -=
                            1 + rand32(ARITH_MAX);

                    } else {

                        uint32_t pos = rand32(fuzz_data.size() - 1);
                        uint16_t num = 1 + rand32(ARITH_MAX);

                        *(uint16_t*)(fuzz_data.data() + pos) = SWAP16(
                            SWAP16(*(uint16_t*)(fuzz_data.data() + pos)) - num);
                    }

                    break;
                }

                case 7: {

                    /* Randomly add to word, random endian. */

                    if (fuzz_data.size() < 2)
                        break;

                    if (rand32(2)) {

                        uint32_t pos = rand32(fuzz_data.size() - 1);

                        *(uint16_t*)(fuzz_data.data() + pos) +=
                            1 + rand32(ARITH_MAX);

                    } else {

                        uint32_t pos = rand32(fuzz_data.size() - 1);
                        uint16_t num = 1 + rand32(ARITH_MAX);

                        *(uint16_t*)(fuzz_data.data() + pos) = SWAP16(
                            SWAP16(*(uint16_t*)(fuzz_data.data() + pos)) + num);
                    }

                    break;
                }

                case 8: {

                    /* Randomly subtract from dword, random endian. */

                    if (fuzz_data.size() < 4)
                        break;

                    if (rand32(2)) {

                        uint32_t pos = rand32(fuzz_data.size() - 3);

                        *(uint32_t*)(fuzz_data.data() + pos) -=
                            1 + rand32(ARITH_MAX);

                    } else {

                        uint32_t pos = rand32(fuzz_data.size() - 3);
                        uint32_t num = 1 + rand32(ARITH_MAX);

                        *(uint32_t*)(fuzz_data.data() + pos) = SWAP32(
                            SWAP32(*(uint32_t*)(fuzz_data.data() + pos)) - num);
                    }

                    break;
                }

                case 9: {

                    /* Randomly add to dword, random endian. */

                    if (fuzz_data.size() < 4)
                        break;

                    if (rand32(2)) {

                        uint32_t pos = rand32(fuzz_data.size() - 3);

                        *(uint32_t*)(fuzz_data.data() + pos) +=
                            1 + rand32(ARITH_MAX);

                    } else {

                        uint32_t pos = rand32(fuzz_data.size() - 3);
                        uint32_t num = 1 + rand32(ARITH_MAX);

                        *(uint32_t*)(fuzz_data.data() + pos) = SWAP32(
                            SWAP32(*(uint32_t*)(fuzz_data.data() + pos)) + num);
                    }

                    break;
                }

                case 10: {

                    /* Just set a random byte to a random value. Because,
                      why not. We use XOR with 1-255 to eliminate the
                      possibility of a no-op. */

                    uint32_t pos = rand32(fuzz_data.size());
                    fuzz_data[pos] = static_cast<std::byte>(
                        static_cast<char>(fuzz_data[pos]) ^
                        static_cast<char>(1 + rand32(255)));
                    break;
                }

                case 11 ... 12: {

                    /* Delete bytes. We're making this a bit more likely
                        than insertion (the next option) in hopes of keeping
                        files reasonably small. */

                    uint32_t del_from, del_len;

                    if (fuzz_data.size() <= minLen)
                        break;

                    /* Don't delete too much. */

                    del_len = choose_block_len(fuzz_data.size() - minLen);

                    del_from = rand32(fuzz_data.size() - del_len + 1);

                    memmove(fuzz_data.data() + del_from,
                            fuzz_data.data() + del_from + del_len,
                            fuzz_data.size() - del_from - del_len);

                    break;
                }

                case 13: {

                    if (fuzz_data.size() + HAVOC_BLK_XL < MAX_FILE) {

                        /* Clone bytes (75%) or insert a block of constant bytes (25%). */

                        uint8_t actually_clone = rand32(4);
                        uint32_t clone_from, clone_to, clone_len;
                        std::vector<std::byte> new_buf = fuzz_data;
                        uint32_t max_blk_len = fuzz_data.size() < maxLen
                                                   ? fuzz_data.size()
                                                   : maxLen;

                        if (actually_clone) {
                            clone_len = choose_block_len(max_blk_len);
                            clone_from =
                                rand32(fuzz_data.size() - clone_len + 1);

                        } else {

                            clone_len = choose_block_len(max_blk_len);
                            clone_from = 0;
                        }

                        uint32_t clone_to_lim =
                            fuzz_data.size() < maxLen - clone_len
                                ? fuzz_data.size()
                                : maxLen - clone_len;
                        clone_to = rand32(clone_to_lim);

                        if (clone_to + clone_len > new_buf.size()) {
                            new_buf.resize(clone_to + clone_len);
                        }

                        /* Inserted part */
                        if (actually_clone)
                            memcpy(new_buf.data() + clone_to,
                                   fuzz_data.data() + clone_from, clone_len);
                        else
                            memset(
                                new_buf.data() + clone_to,
                                rand32(2)
                                    ? rand32(256)
                                    : static_cast<char>(
                                          fuzz_data[rand32(fuzz_data.size())]),
                                clone_len);

                        if (new_buf.size() > maxLen) {
                            int a = 1;
                        }
                        fuzz_data = std::move(new_buf);
                    }

                    break;
                }

                case 14: {

                    /* Overwrite bytes with a randomly selected chunk (75%) or fixed
                      bytes (25%). */

                    uint32_t copy_from, copy_to, copy_len;

                    if (fuzz_data.size() < 2)
                        break;

                    copy_len = choose_block_len(fuzz_data.size() - 1);

                    copy_from = rand32(fuzz_data.size() - copy_len + 1);
                    copy_to = rand32(fuzz_data.size() - copy_len + 1);

                    if (rand32(4)) {

                        if (copy_from != copy_to)
                            memmove(fuzz_data.data() + copy_to,
                                    fuzz_data.data() + copy_from, copy_len);

                    } else
                        memset(fuzz_data.data() + copy_to,
                               rand32(2)
                                   ? rand32(256)
                                   : static_cast<char>(
                                         fuzz_data[rand32(fuzz_data.size())]),
                               copy_len);

                    break;
                }
                case 15: {
                    // Like case 13, but instead of copying only once, it copies a random amount of times to the end.
                    uint8_t actually_clone = rand32(4);
                    uint32_t clone_from, clone_to, clone_len;
                    std::vector<std::byte> new_buf = fuzz_data;
                    uint32_t max_blk_len =
                        std::min(static_cast<int>(fuzz_data.size()), maxLen);

                    if (actually_clone) {
                        clone_len = choose_block_len(max_blk_len);
                        clone_from = rand32(fuzz_data.size() - clone_len + 1);
                    } else {
                        clone_len = choose_block_len(max_blk_len);
                        clone_from = 0;
                    }

                    uint32_t clone_to_lim =
                        std::min(static_cast<uint32_t>(fuzz_data.size()),
                                 maxLen - clone_len);
                    clone_to = rand32(clone_to_lim);

                    uint32_t clone_count =
                        rand32(((maxLen - clone_to) / clone_len) + 1);

                    if (clone_to + clone_len > new_buf.size()) {
                        new_buf.resize(clone_to + clone_len * clone_count);
                    }

                    /* Inserted part */
                    if (actually_clone) {
                        for (int i = 0; i < clone_count; i++) {
                            memcpy(new_buf.data() + clone_to * i,
                                   fuzz_data.data() + clone_from, clone_len);
                        }
                    } else {
                        for (int i = 0; i < clone_count; i++) {
                            memset(
                                new_buf.data() + clone_to * i,
                                rand32(2)
                                    ? rand32(256)
                                    : static_cast<char>(
                                          fuzz_data[rand32(fuzz_data.size())]),
                                clone_len);
                        }
                    }

                    if (new_buf.size() > maxLen) {
                        std::cout << "Error with new buffer size" << std::endl;
                        assert(false);
                        int a = 1;
                    }
                    fuzz_data = std::move(new_buf);

                    break;
                }

                    // /* Values 15 and 16 can be selected only if there are any extras
                    //    present in the dictionaries. */

                    // case 15: {

                    //     /* Overwrite bytes with an extra. */

                    //     if (!extras_cnt || (a_extras_cnt && rand32(2))) {

                    //       /* No user-specified extras or odds in our favor. Let's use an
                    //          auto-detected one. */

                    //       uint32_t use_extra = rand32(a_extras_cnt);
                    //       uint32_t extra_len = a_extras[use_extra].len;
                    //       uint32_t insert_at;

                    //       if (extra_len > size) break;

                    //       insert_at = rand32(size - extra_len + 1);
                    //       memcpy(output.data() + insert_at, a_extras[use_extra].data, extra_len);

                    //     } else {

                    //       /* No auto extras or odds in our favor. Use the dictionary. */

                    //       uint32_t use_extra = rand32(extras_cnt);
                    //       uint32_t extra_len = extras[use_extra].len;
                    //       uint32_t insert_at;

                    //       if (extra_len > size) break;

                    //       insert_at = rand32(size - extra_len + 1);
                    //       memcpy(output.data() + insert_at, extras[use_extra].data, extra_len);

                    //     }

                    //     break;

                    //   }

                    // case 16: {

                    //     uint32_t use_extra, extra_len, insert_at = rand32(size + 1);
                    //     uint8_t* new_buf;

                    //     /* Insert an extra. Do the same dice-rolling stuff as for the
                    //        previous case. */

                    //     if (!extras_cnt || (a_extras_cnt && rand32(2))) {

                    //       use_extra = rand32(a_extras_cnt);
                    //       extra_len = a_extras[use_extra].len;

                    //       if (size + extra_len >= MAX_FILE) break;

                    //       new_buf = ck_alloc_nozero(size + extra_len);

                    //       /* Head */
                    //       memcpy(new_buf, output.data(), insert_at);

                    //       /* Inserted part */
                    //       memcpy(new_buf + insert_at, a_extras[use_extra].data, extra_len);

                    //     } else {

                    //       use_extra = rand32(extras_cnt);
                    //       extra_len = extras[use_extra].len;

                    //       if (size + extra_len >= MAX_FILE) break;

                    //       new_buf = ck_alloc_nozero(size + extra_len);

                    //       /* Head */
                    //       memcpy(new_buf, output.data(), insert_at);

                    //       /* Inserted part */
                    //       memcpy(new_buf + insert_at, extras[use_extra].data, extra_len);

                    //     }

                    //     /* Tail */
                    //     memcpy(new_buf + insert_at + extra_len, output.data() + insert_at,
                    //            size - insert_at);

                    //     ck_free(output);
                    //     output   = new_buf;
                    //     size += extra_len;

                    //     break;

                    // }
            }

            if (fuzz_data.size() > maxLen) {
                int a = 1;
            }
        }
    }
}