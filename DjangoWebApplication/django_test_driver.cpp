#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstddef> // For std::byte
#include <string>
enum class FieldTypes {
    STRING,
    INTEGER,
    BINARY
};
typedef struct {
    unsigned int minLen;
    unsigned int maxLen;
    FieldTypes type;
    std::string name;
    std::vector<std::vector<std::byte>> validChoices;
} Field;

typedef struct {
    Field format;
    std::vector<std::byte> data;
    unsigned int energy;
} Input;

using namespace utility; // Common utilities like string conversions
using namespace web; // Common features like URIs.
using namespace web::http; // Common HTTP functionality
using namespace web::http::client; // HTTP client features


// Function to convert a string method to an http::method (string_t)
method string_to_method(const std::string& method_str) {
    if (method_str == "POST") return methods::POST;
    if (method_str == "PUT") return methods::PUT;
    if (method_str == "DELETE") return methods::DEL;
    return methods::GET; // Default to GET
}
void test_driver(const Input& input) {
    // Parse the format.name for method and URL
    std::istringstream iss(input.format.name);
    std::string method_str, url_str;
    iss >> method_str >> url_str;

    // Convert the data vector<std::byte> to a string for payload
    std::string payload(reinterpret_cast<const char*>(input.data.data()), input.data.size());


    // Convert string method to http::method
    method http_method = string_to_method(method_str);

    // Create the HTTP client and request using the extracted method and URL
    http_client client(U(url_str));
    http_request request(http_method);

    // Assuming JSON payload, set the Content-Type header accordingly
    request.headers().add(U("Content-Type"), U("application/json"));

    // Set the payload if not empty
    if (!payload.empty()) {
        request.set_body(payload);
    }

    // Send the request and process the response as before
    client.request(request)
        .then([](http_response response) {
            std::cout << "Status Code: " << response.status_code() << std::endl;
            if(response.status_code() == status_codes::OK) {
                return response.extract_string();
            }
            return pplx::task_from_result(utility::string_t());
        })
        .then([](pplx::task<utility::string_t> previousTask) {
            try {
                utility::string_t body = previousTask.get();
                std::cout << "Response Body: " << body << std::endl;
            }
            catch (const http_exception& e) {
                std::cerr << "HTTP exception: " << e.what() << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Standard exception: " << e.what() << std::endl;
            }
        })
        .wait();
}

int main() {
    // Correctly initializing a vector of std::byte for the payload
    std::vector<std::byte> payloadBytes = {
        std::byte('{'), std::byte('"'), std::byte('k'), std::byte('e'), std::byte('y'), std::byte('"'),
        std::byte(':'), std::byte('"'), std::byte('v'), std::byte('a'), std::byte('l'), std::byte('u'),
        std::byte('e'), std::byte('"'), std::byte('}')
    };

    // Correct initialization of your Input struct
    Input exampleInput = {
        Field{0, 0, FieldTypes::STRING, "GET http://127.0.0.1:8000/", {}},
        payloadBytes,
        0
    };

    // Your function call here
    test_driver(exampleInput);

    return 0;
}