#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

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

int main() {
    std::ifstream inputFile("fuzzed_input.txt");
    if (!inputFile.is_open()) {
        std::cerr << "Could not open the fuzzed input file." << std::endl;
        return -1;
    }

    std::string method_str;
    std::getline(inputFile, method_str);

    std::string url_str;
    std::getline(inputFile, url_str);

    std::string payload;
    std::string line;
    while (std::getline(inputFile, line)) {
        payload += line + "\n";
    }

    // Trim the last newline if needed
    if (!payload.empty() && payload.back() == '\n') {
        payload.pop_back();
    }

    inputFile.close(); // Close the file as it's no longer needed.

    // Convert string method to http::method (string_t)
    method http_method = string_to_method(method_str);

    // Create the HTTP client and request using the http::method (string_t)
    http_client client(U(url_str));
    http_request request(http_method); // Use the method directly here

    // Set headers and body if necessary
    request.headers().add(U("Content-Type"), U("application/json")); // Assuming JSON payload

    if (!payload.empty()) {
        request.set_body(payload);
    }

    // Send the HTTP request asynchronously and process the response
    client.request(request)
        .then([](http_response response) {
            // Check the status code
            std::cout << "Status Code: " << response.status_code() << std::endl;

            // If status code indicates success, extract the string
            if(response.status_code() == status_codes::OK) {
                return response.extract_string();
            }
            // If not successful, return an empty string
            return pplx::task_from_result(utility::string_t());
        })
        .then([](pplx::task<utility::string_t> previousTask) {
            try {
                // Output the response body
                utility::string_t body = previousTask.get();
                std::cout << "Response Body: " << body << std::endl;
            }
            catch (const http_exception& e) {
                // Print an error message when an exception occurs
                std::cerr << "HTTP exception: " << e.what() << std::endl;
            }
            catch (const std::exception& e) {
                // Print an error message when a standard exception occurs
                std::cerr << "Standard exception: " << e.what() << std::endl;
            }
        })
        .wait(); // Wait for all the outstanding asynchronous operations to complete

    return 0;
}
