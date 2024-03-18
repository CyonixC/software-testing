#include <vector>
#include <cstddef>  // For std::byte
#include <filesystem>   // For file paths
#include <fstream>  // For reading files
#include <iostream> // For some basic debugging
#include "json.hpp"
#include "inputs.h"

using json = nlohmann::json;
namespace fs = std::filesystem;


/**
 * @brief Read the choices from a folder.
 * @details The folder is expected to contain n files for n possible choices.
 * @param p The path to the folder to read from.
*/
std::vector<std::vector<std::byte>> readChoices(const fs::path& p) {
    std::vector<std::vector<std::byte>> choices;
    char c;
    for (const auto& entry : fs::directory_iterator(p)) {
        std::ifstream file(entry.path(), std::ios::binary);
        std::vector<std::byte> choice;
        while (file.get(c))
            choice.push_back(static_cast<std::byte>(c));
        choices.push_back(choice);
        file.close();
        }

    return choices;
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
    for (auto &it : json_fields.items()) {
        json field_conf = it.value();
        Field f;
        f.minLen = field_conf["min_length"];
        f.maxLen = field_conf["max_length"];

        // If the field uses choices, read the choices from the file
        if (field_conf["use_choices"]) {

            // If the file is not found, throw an error
            if (p == "") {
                throw std::runtime_error("There is a field that uses choices, but no choice_folder is specified in the JSON");
            }

            // Read the files and store the choices
            f.validChoices = readChoices(p/fs::path(it.key()));
        }
        fields.push_back(f);
    }
    return fields;
}

std::vector<Input> readSeed(const fs::path& filepath) {
    std::vector<Input> inputs;
    // std::ifstream file(filepath);
    // json j = json::parse(file);
    // for (auto &it : j.items()) {
    // }
    return inputs;
}

int main() {
    std::string config_file = "input_config_example.json";
    std::ifstream file(config_file);
    json j = json::parse(file);
    std::vector<Field> fields = readFields(j);
    for (Field f : fields) {
        std::cout << "Min length: " << f.minLen << std::endl;
        std::cout << "Max length: " << f.maxLen << std::endl;
        std::cout << "Choices: " << std::endl;
        for (std::vector<std::byte> choice : f.validChoices) {
            for (std::byte c : choice) {
                std::cout << static_cast<char>(c);
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
