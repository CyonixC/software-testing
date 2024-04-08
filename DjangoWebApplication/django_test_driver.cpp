#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <iostream>
#include <vector>
#include <cstddef> // For std::byte
#include <string>
#include <chrono>
#include <thread>
#include "../driver.h"

using namespace utility; // Common utilities like string conversions
using namespace web; // Common features like URIs.
using namespace web::http; // Common HTTP functionality
using namespace web::http::client; // HTTP client features

// Convert a string method to an http::method
method string_to_method(const std::string& method_str) {
    if (method_str == "POST") return methods::POST;
    if (method_str == "PUT") return methods::PUT;
    if (method_str == "DELETE") return methods::DEL;
    return methods::GET; // Default to GET
}

// Converts std::vector<std::byte> to std::string
std::string bytes_to_string(const std::vector<std::byte>& data) {
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

void test_driver(const std::vector<Input>& inputs) {
    std::string method_str, url_str, payload;

    // Extract method, URL, and payload from inputs
    for (const auto& input : inputs) {
        if (input.name == "Method") {
            method_str = bytes_to_string(input.data);
        } else if (input.name == "URL") {
            url_str = bytes_to_string(input.data);
        } else if (input.name == "Payload") {
            payload = bytes_to_string(input.data);
        }
    }

    method http_method = string_to_method(method_str);

    // Create HTTP client and request
    http_client client(U(url_str));
    http_request request(http_method);

    request.headers().add(U("Content-Type"), U("application/json"));
    if (!payload.empty()) {
        request.set_body(payload);
    }

    // Initialize timer for timeout
    auto requestStartTime = std::chrono::steady_clock::now();
    bool timeoutOccurred = false;

    // Send the request
    client.request(request)
        .then([&timeoutOccurred, requestStartTime](http_response response) -> pplx::task<std::string> {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - requestStartTime).count();
            if(duration > 10) { // Check for timeout (10 seconds)
                timeoutOccurred = true;
                throw std::runtime_error("Timeout occurred.");
            }
            std::cout << "Status Code: " << response.status_code() << std::endl;
            if(response.status_code() == status_codes::OK) {
                return response.extract_string(true);
            }
            return pplx::task_from_result(std::string());
        })
        .then([&timeoutOccurred](pplx::task<std::string> previousTask) {
            try {
                if(!timeoutOccurred) {
                    auto body = previousTask.get();
                    std::cout << "Response Body: " << body << std::endl;
                }
            } catch (const http_exception& e) {
                std::cerr << "HTTP exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Standard exception: " << e.what() << std::endl;
            }
        })
        .wait();
    
    if(timeoutOccurred) {
        std::cout << "Request timed out." << std::endl;
    }
}

int main() {
    std::vector<Input> inputs = {
        {{std::byte('G'), std::byte('E'), std::byte('T')}, "Method"},
        {{std::byte('h'), std::byte('t'), std::byte('t'), std::byte('p'), std::byte(':'), std::byte('/'), std::byte('/'), std::byte('1'), std::byte('2'), std::byte('7'), std::byte('.'), std::byte('0'), std::byte('.'), std::byte('0'), std::byte('.'), std::byte('1'), std::byte(':'), std::byte('8'), std::byte('0'), std::byte('0'), std::byte('0'), std::byte('/')}, "URL"},
        {}, // Empty payload for a GET request
    };

    test_driver(inputs);

    return 0;
}