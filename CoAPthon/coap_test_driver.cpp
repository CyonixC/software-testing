#include <coap3/coap.h> // Include the libcoap library for CoAP functionality
#include <arpa/inet.h>  // For network functions like inet_pton
#include <cstdio>       // For printf
#include <fstream>      // For file I/O
#include <sstream>      // For string stream processing
#include <string>       // For std::string
#include <iostream>     // For standard I/O
#include <algorithm>    // For std::transform, used in converting strings to uppercase

coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *sent, const coap_pdu_t *received, const coap_mid_t mid) {
    const uint8_t* data = nullptr;
    size_t data_len;
    // Try to extract the data from the received PDU
    if (coap_get_data(received, &data_len, &data)) {
        printf("Received: %.*s\n", (int)data_len, data); // Print the payload of the response
    } else {
        coap_pdu_code_t response_code = coap_pdu_get_code(received);
        printf("Received response with code: %d\n", response_code); // Or just print the response code
    }
    return COAP_RESPONSE_OK; // Indicate successful handling of the response
}

// Function to convert a string to uppercase
// Converts a std::string to uppercase
void to_uppercase(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); }); // Apply toupper to each character
}
int main(int argc, char **argv) {
    std::ifstream input("seed_input.txt"); // Open the input file
    if (!input.is_open()) {
        std::cerr << "Failed to open seed_input.txt" << std::endl;
        return 1; // Exit if the file cannot be opened
    }

    // Read the method, path, and optionally a payload from the input file
    std::string method, path;
    std::getline(input, method, ' '); // Read the method
    to_uppercase(method); // Convert the method to uppercase for consistent handling
    std::getline(input, path); // Read the rest of the line as the path

    std::string payload;
    // Assuming payload is needed for methods other than GET and DELETE
    if (method != "GET" && method != "DELETE") {
        std::getline(input, payload); // Read the next line as payload, if necessary
    }
    input.close(); // Close the input file

    // Initialize libcoap
    coap_startup();

    // Create a new CoAP context
    coap_context_t *ctx = coap_new_context(nullptr);
    if (!ctx) {
        coap_cleanup();
        return 1; // Exit if the context cannot be created
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
        coap_free_context(ctx);
        coap_cleanup();
        return 1; // Exit if the session cannot be created
    }

    // Create the PDU based on the method read from the file
    coap_pdu_t *pdu = nullptr;
    if (method == "GET") {
        // Initialize a GET request PDU
        pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET, coap_new_message_id(session), coap_session_max_pdu_size(session));
    } 
    else if (method == "PUT") {
            // Initialize a PUT request PDU and add the payload if present
            pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_PUT, coap_new_message_id(session), coap_session_max_pdu_size(session));
            if (!payload.empty()) {
            // Add payload to the PDU for the PUT request
            coap_add_data(pdu, payload.length(), reinterpret_cast<const uint8_t*>(payload.c_str()));
        }
    }
    else if(method=="POST") {
            // Initialize a PUT request PDU and add the payload if present
            pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_CODE_POST, coap_new_message_id(session), coap_session_max_pdu_size(session));
            if (!payload.empty()) {
            // Add payload to the PDU for the PUT request
            coap_add_data(pdu, payload.length(), reinterpret_cast<const uint8_t*>(payload.c_str()));
        }
    }
// Extend this block to handle other methods like POST, DELETE, etc., in a similar manner
if (!pdu) {
    std::cerr << "Failed to create PDU for method: " << method << std::endl;
    coap_session_release(session);
    coap_free_context(ctx);
    coap_cleanup();
    return 1; // Exit if the PDU could not be created
}

// Add the URI path option to the PDU
coap_str_const_t *uri_path = coap_make_str_const(path.c_str());
coap_add_option(pdu, COAP_OPTION_URI_PATH, uri_path->length, uri_path->s);

// Register the response handler for the request
coap_register_response_handler(ctx, response_handler);

// Send the request
coap_send(session, pdu);

// Enter a loop to process incoming responses. Adjust the timeout as needed.
while (1) {
    int result = coap_io_process(ctx, COAP_IO_WAIT);
    if (result < 0) {
        break; // Exit the loop on error
    }
}

// Cleanup
coap_session_release(session);
coap_free_context(ctx);
coap_cleanup();

return 0;
}