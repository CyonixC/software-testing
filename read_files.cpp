#include <vector>
#include <cstddef>  // For std::byte
#include <filesystem>   // For file paths
#include <fstream>  // For reading files
#include <iostream> // For some basic debugging
#include <limits.h> // For UINT_MAX
#include "json.hpp"
#include "inputs.h"

using json = nlohmann::json;
namespace fs = std::filesystem;


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
        
        // Assign min and max length values, if they exist
        if (field_conf.contains("min_length")) {
            f.minLen = field_conf["min_length"];
        } else {
            f.minLen = 0;
        }

        if (field_conf.contains("max_length")) {
            f.maxLen = field_conf["max_length"];
        } else {
            f.maxLen = UINT_MAX;
        }

        f.name = it.key();

        // If the field uses choices, read the choices from the file
        if (field_conf.contains("choices")) {

            FieldTypes type;
            // Check the type of the choices
            if (field_conf["choice_type"] == "string") {
                type = FieldTypes::STRING;
            } else if (field_conf["choice_type"] == "integer") {
                type = FieldTypes::INTEGER;
            } else if (field_conf["choice_type"] == "binary") {
                type = FieldTypes::BINARY;
            } else {
                throw std::runtime_error("Invalid choice type");
            }

            json choices = field_conf["choices"];

            for (auto &it : choices.items()) {
                // Read the items
                switch (type) {
                case FieldTypes::STRING: 
                {
                    std::string val = it.value().get<std::string>();
                    std::vector<std::byte> vec;
                    for (char c : val)  
                        vec.push_back(static_cast<std::byte>(c));

                    f.validChoices.push_back(vec);
                    break;
                }
                case FieldTypes::INTEGER:
                {
                    int val = it.value().get<int>();
                    f.validChoices.push_back(std::vector<std::byte>(reinterpret_cast<std::byte*>(&val), reinterpret_cast<std::byte*>(&val) + sizeof(int)));
                    break;
                }
                case FieldTypes::BINARY:
                {
                    throw std::runtime_error("Binary choices not yet implemented");
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

InputSeed readSeed(const json &j, std::vector<Field> &fields) {
    InputSeed ret;
    ret.energy = 10;
    for (Field f : fields) {
        if (!j.contains(f.name))
            throw std::runtime_error("Seed does not contain required field value");
        InputField inp;
        inp.format = f;
        FieldTypes type =  f.type;
        switch (type) {
            case FieldTypes::STRING: 
            {
                std::string val = j[f.name]["data"].get<std::string>();
                std::vector<std::byte> vec;
                for (char c : val)  
                    vec.push_back(static_cast<std::byte>(c));

                inp.data = vec;
                break;
            }
            case FieldTypes::INTEGER:
            {
                int val = j[f.name]["data"].get<int>();
                inp.data = std::vector<std::byte>(reinterpret_cast<std::byte*>(&val), reinterpret_cast<std::byte*>(&val) + sizeof(int));
                break;
            }
            case FieldTypes::BINARY:
            {
                throw std::runtime_error("Binary choices not yet implemented");
                break;
            }
            default:
                break;
        }
        ret.inputs.push_back(inp);
    }
    return ret;
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
                std::cout << static_cast<char>(c) << std::endl;
            }
            std::cout << std::endl;
        }
    }
    return 0;
}
