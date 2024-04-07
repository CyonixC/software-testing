#include <coap3/coap.h> // Include the libcoap library for CoAP functionality
#include <arpa/inet.h>  // For network functions like inet_pton
#include <cstdio>       // For printf
#include <fstream>      // For file I/O
#include <sstream>      // For string stream processing
#include <string>       // For std::string
#include <iostream>     // For standard I/O
#include <algorithm>    // For std::transform, used in converting strings to uppercase


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



coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received, const coap_mid_t mid) {
    const uint8_t* data = nullptr;
    size_t data_len;
    if (coap_get_data(received, &data_len, &data)) {
        printf("Received: %.*s\n", (int)data_len, data);
    } else {
        coap_pdu_code_t response_code = coap_pdu_get_code(received);
        printf("Received response with code: %d\n", response_code);
    }
    return COAP_RESPONSE_OK;
}

void to_uppercase(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::toupper(c); });
}
int test_driver(const Input &input) {
    // Initialize libcoap
    coap_startup();

    // Create a new CoAP context
    coap_context_t *ctx = coap_new_context(nullptr);
    if (!ctx) {
        std::cerr << "Failed to create CoAP context\n";
        coap_cleanup();
        return 1; // Early return if context creation fails
    }

    // Set up the destination address (server)
    const char *host = "127.0.0.1";
    const uint16_t port = 5683;
    coap_address_t dst_addr;
    coap_address_init(&dst_addr);
    dst_addr.addr.sin.sin_family = AF_INET;
    dst_addr.addr.sin.sin_port = htons(port);
    inet_pton(AF_INET, host, &dst_addr.addr.sin.sin_addr);

    // Create a new CoAP session
    coap_session_t *session = coap_new_client_session(ctx, nullptr, &dst_addr, COAP_PROTO_UDP);
    if (!session) {
        std::cerr << "Failed to create CoAP session\n";
        coap_free_context(ctx);
        coap_cleanup();
        return 1; // Early return if session creation fails
    }

    // Extract method and path from input.format.name
    std::string fullName = input.format.name;
    size_t delimiterPos = fullName.find("/");
    if (delimiterPos == std::string::npos) {
        std::cerr << "Invalid format name. Expected 'METHOD/PATH' format.\n";
        coap_free_context(ctx);
        coap_cleanup();
        return 1; // Early exit on error
    }

    std::string method = fullName.substr(0, delimiterPos);
    to_uppercase(method); // Ensure method is uppercase
    std::string path = fullName.substr(delimiterPos + 1); // Extract path
    coap_pdu_t *pdu = nullptr;
    if (method == "GET") {
        pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, coap_new_message_id(session), coap_session_max_pdu_size(session));
    } else if (method == "PUT") {
        pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_PUT, coap_new_message_id(session), coap_session_max_pdu_size(session));
    } else if (method == "POST") {
        pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST, coap_new_message_id(session), coap_session_max_pdu_size(session));
    } else {
        std::cerr << "Unsupported method: " << method << "\n";
        coap_session_release(session);
        coap_free_context(ctx);
        coap_cleanup();
        return 1; // Method not supported, early return
    }

    // Add payload if method is PUT or POST and payload is not empty
    std::string payload(reinterpret_cast<const char*>(input.data.data()), input.data.size());
    if (!payload.empty() && (method == "PUT" || method == "POST")) {
        coap_add_data(pdu, payload.length(), reinterpret_cast<const uint8_t*>(payload.c_str()));
    }

    // Add the URI path option to the PDU using the extracted path
    coap_add_option(pdu, COAP_OPTION_URI_PATH, path.length(), reinterpret_cast<const uint8_t*>(path.c_str()));

    // Register the response handler and send the request
    coap_register_response_handler(ctx, response_handler);
    coap_send(session, pdu);

    // Process incoming responses
    while (coap_io_process(ctx, 1000) > 0) {
            // This loop processes incoming packets and handles retransmissions.
        }

    
    // Cleanup
    coap_session_release(session);
    coap_free_context(ctx);
    coap_cleanup();

    return 0; // Indicate success
}

int main() {
    // Construct an example Input structure for a GET request to "/example/path"
    Input exampleInput = {
        // Initialize the format field with the method and path as "GET/example/path"
        // Assuming other fields of Field are not critical for this example, they are left as default or empty
        {0, 0, FieldTypes::STRING, "GET/", {}}, // Example Field initialization with method/path
        std::vector<std::byte>(), // No payload for a GET request
        0 // Placeholder for energy, assuming it's not used in this context
    };

    // Call the test_driver function with the constructed Input
    int result = test_driver(exampleInput);

    // Check the result of the test driver execution
    std::cout << result;
    if (result == 0) {
        std::cout << "Request sent and processed successfully.\n";
    } else {
        std::cout << "An error occurred during request processing.\n";
    }

return result;
}