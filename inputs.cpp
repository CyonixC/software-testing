#include <vector>
#include <cstddef>  // For std::byte
#include "inputs.h"
#include "config.h"
#include "json.hpp"


/**
 * @brief Converts an InputSeed to a json representation.
*/
json InputSeed::to_json() const {
    json out;
    for (InputField input : inputs) {
        std::string name = input.format.name;
        switch (input.format.type)
        {
        case FieldTypes::STRING: {
            std::string val {name.begin(), name.end()};
            out[name] = val;
            break;
        }
        case FieldTypes::INTEGER: {
            int val = static_cast<int>((input.data[3] << 24) | (input.data[2] << 16) | (input.data[1] << 8) | input.data[0]);
            out[name] = val;
            break;
        }
        case FieldTypes::BINARY: {
            std::vector<uint8_t> int_vector = binary_to_int(input.data);
            out[name] = int_vector;
            break;
        }
        default:
            break;
        }
    }
    return out;
}

/**
 * @brief Converts an Input vector to a json representation.
*/
json inputVectorToJSON(const std::vector<Input>& input) {
    json out;
    for (Input input : input) {
        std::string name = input.name;
        std::vector<uint8_t> int_vector = binary_to_int(input.data);
        out[name] = int_vector;
    }
    return out;
}