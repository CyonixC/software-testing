#include "config.h"
#include <limits.h>    // For INT_MAX
#include <cstddef>     // For std::byte
#include <filesystem>  // For file paths
#include <fstream>     // For reading files
#include <iostream>    // For some basic debugging
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Reads a binary given its representation in json.
 * This binary should have been produced by the write_json_binary() function.
 * 
 * @param json_bin A std::vector of uint8s to convert to bytes.
*/
std::vector<std::byte> int_to_binary(const std::vector<uint8_t>& json_bin) {
    std::vector<std::byte> ret;
    for (auto byte_dec : json_bin) {
        ret.push_back(static_cast<std::byte>(byte_dec));
    }

    return ret;
}

/**
 * @brief Converts a std::vector of bytes to integer representation.
*/
std::vector<uint8_t> binary_to_int(const std::vector<std::byte>& byte_arr) {
    std::vector<uint8_t> ret;
    for (auto byte_bin : byte_arr) {
        ret.push_back(static_cast<uint8_t>(byte_bin));
    }

    return ret;
}

/**
 * @brief Read the fields from the config JSON.
 * @details The JSON is expected to have a "fields" key, which is an 
 * array of expected outputs from the fuzzer.
 * @param j The JSON object to read from.
*/
std::vector<Field> readFields(const json& j) {
    std::vector<Field> fields;
    fs::path p = "";
    if (!j.contains("fields")) {
        throw std::runtime_error("No fields found in JSON");
    }
    json json_fields = j["fields"];
    if (j.contains("choice_folder")) {
        p = j["choice_folder"].get<std::string>();
    }
    for (auto& it : json_fields.items()) {
        json field_conf = it.value();
        Field f;

        f.name = it.key();

        FieldTypes type;
        // Check the type of the choices
        if (field_conf["type"] == "string") {
            type = FieldTypes::STRING;
        } else if (field_conf["type"] == "integer") {
            type = FieldTypes::INTEGER;
        } else if (field_conf["type"] == "binary") {
            type = FieldTypes::BINARY;
        } else {
            throw std::runtime_error("Invalid choice type");
        }

        f.type = type;

        // If the field specifies min / max length, use those; otherwise, assume
        // 1 min and inf max

        if (field_conf.contains("min_length")) {
            f.minLen = field_conf["min_length"];
        } else {
            f.minLen = 1;
        }

        if (field_conf.contains("max_length")) {
            f.maxLen = field_conf["max_length"];
        } else {
            f.maxLen = INT_MAX;
        }

        // If the field uses choices, read the choices from the file
        if (field_conf.contains("choices")) {
            json choices = field_conf["choices"];

            for (auto& it : choices.items()) {
                // Read the items
                switch (type) {
                    case FieldTypes::STRING: {
                        std::string val = it.value().get<std::string>();
                        std::vector<std::byte> vec;
                        for (char c : val)
                            vec.push_back(static_cast<std::byte>(c));

                        f.validChoices.push_back(vec);
                        break;
                    }
                    case FieldTypes::INTEGER: {
                        int val = it.value().get<int>();
                        f.validChoices.push_back(std::vector<std::byte>(
                            reinterpret_cast<std::byte*>(&val),
                            reinterpret_cast<std::byte*>(&val) + sizeof(int)));
                        break;
                    }
                    case FieldTypes::BINARY: {
                        const std::vector<uint8_t> val = it.value();
                        f.validChoices.push_back(int_to_binary(val));
                        break;
                    }
                    default:
                        break;
                }
            }
        }
        fields.push_back(f);
    }
    return fields;
}

InputSeed readSeed(const json& j, std::vector<Field>& fields) {
    InputSeed ret;
    for (Field f : fields) {
        if (!j.contains(f.name))
            throw std::runtime_error(
                "Seed does not contain required field value");
        InputField inp;
        inp.format = f;
        FieldTypes type = f.type;
        switch (type) {
            case FieldTypes::STRING: {
                std::string val = j[f.name].get<std::string>();
                std::vector<std::byte> vec;
                for (char c : val)
                    vec.push_back(static_cast<std::byte>(c));

                inp.data = vec;
                break;
            }
            case FieldTypes::INTEGER: {
                int val = j[f.name].get<int>();
                inp.data = std::vector<std::byte>(
                    reinterpret_cast<std::byte*>(&val),
                    reinterpret_cast<std::byte*>(&val) + sizeof(int));
                break;
            }
            case FieldTypes::BINARY: {
                const std::vector<uint8_t> val = j[f.name];
                inp.data = int_to_binary(val);
                break;
            }
            default:
                break;
        }
        ret.inputs.push_back(inp);
    }
    return ret;
}

// int main() {
//     // const std::vector<unsigned char> test{13, 44, 25, 56, 165, 125, 43, 29, 38};
//     // const std::vector<std::byte> bytes = int_to_binary(test);
//     // json j;
//     // j["test"] = test;
//     // std::cout << j.dump() << std::endl;
//     const std::string config_file = "input_config_example.json";
//     const std::string seed_file = "seed_file_example.json";
//     std::ifstream file(config_file);
//     json j = json::parse(file);
//     std::vector<Field> fields = readFields(j);
//     std::ifstream seed(seed_file);
//     json k = json::parse(seed);
//     InputSeed inputs = readSeed(k, fields);
//     std::cout << inputs.to_json().dump() << std::endl;
//     // for (Field f : fields) {
//     //     std::cout << "Min length: " << f.minLen << std::endl;
//     //     std::cout << "Max length: " << f.maxLen << std::endl;
//     //     std::cout << "Choices: " << std::endl;
//     //     for (std::vector<std::byte> choice : f.validChoices) {
//     //         for (std::byte c : choice) {
//     //             std::cout << static_cast<char>(c) << std::endl;
//     //         }
//     //         std::cout << std::endl;
//     //     }
//     // }
//     return 0;
// }