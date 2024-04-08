#include <iostream>
#include <vector>
#include <cstddef> // For std::byte
#include <cstdint> // For uint8_t
#include <string>
#include <arpa/inet.h> // For inet_pton and htons
#include <sys/socket.h> // For socket, sendto, and close
#include <unistd.h> // For close
#include <cstring> // For memset

// Input structure containing data and its associated name.
struct Input {
    std::vector<std::byte> data;
    std::string name;
};

// Function to construct the CoAP message using the Input vector.
std::vector<uint8_t> createCoapMessage(const std::vector<Input>& inputs) {
    std::vector<uint8_t> message;
    // The first byte of the CoAP header includes version, type, and token length (TKL).
    uint8_t header = 0x00; // Will be built up from the inputs.

    for (const auto& input : inputs) {
        if (input.name == "Version") {
            header |= (std::to_integer<uint8_t>(input.data[0]) << 6);
        } else if (input.name == "Type") {
            header |= (std::to_integer<uint8_t>(input.data[0]) << 4);
        } else if (input.name == "TKL") {
            header |= std::to_integer<uint8_t>(input.data[0]);
        } else if (input.name == "Code") {
            message.push_back(std::to_integer<uint8_t>(input.data[0]));
        } else if (input.name == "MessageID") {
            for (auto& b : input.data) {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        } else if (input.name == "Token") {
            for (auto& b : input.data) {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        } else if (input.name == "Uri-Path") {
            // Assuming Uri-Path is a single option and input data does not include option header.
            message.push_back(0xB0 | 0x0B); // Option delta for Uri-Path
            message.push_back(input.data.size()); // Option length
            for (auto& b : input.data) {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        }
        else if (input.name == "Payload") {
            for (auto& b : input.data) {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        } 
        // Include additional cases as needed for other CoAP message components.
    }
    
    // Prepend the built header to the message.
    message.insert(message.begin(), header);

    return message;
}

// Function to send the message over UDP.
void sendUdpMessage(const std::string& host, uint16_t port, const std::vector<uint8_t>& message) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Could not create UDP socket." << std::endl;
        return;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); // Corrected line
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported." << std::endl;
        close(sockfd);
        return;
    }

    if (sendto(sockfd, message.data(), message.size(), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        std::cerr << "Could not send message." << std::endl;
        close(sockfd);
        return;
    }

    std::cout << "Message sent successfully to " << host << ":" << port << std::endl;

    close(sockfd);
}

int main() {
    // Define the CoAP server details.
    std::string coapServerHost = "127.0.0.1";
    uint16_t coapServerPort = 5683;

    // Define the example inputs for the various parts of the CoAP message.
    std::vector<Input> inputs = {
        {std::vector<std::byte>{std::byte(0x01)}, "Version"}, // CoAP version (01)
        {std::vector<std::byte>{std::byte(0x00)}, "Type"},    // Type (Confirmable: 0)
        {std::vector<std::byte>{std::byte(0x04)}, "TKL"},     // Token Length: 4
        {std::vector<std::byte>{std::byte(0x01)}, "Code"},    // Code: GET (0.01)
        {std::vector<std::byte>{std::byte(0xC4), std::byte(0x09)}, "MessageID"}, // Message ID (0xC409)
        {std::vector<std::byte>{std::byte(0x74), std::byte(0x65), std::byte(0x73), std::byte(0x74)}, "Token"}, // Token ('test')
        {std::vector<std::byte>{std::byte('e'), std::byte('x'), std::byte('a'), std::byte('m'), std::byte('p'), std::byte('l'), std::byte('e')}, "Uri-Path"}, // Uri-Path: 'example'
        //{std::vector<std::byte>{std::byte(0xC4),  std::byte(0x19)}, "Payload"}// Message ID (0xC409)

    };

    // Create the CoAP message.
    std::vector<uint8_t> coapMessage = createCoapMessage(inputs);

    // Send the CoAP message over UDP.
    sendUdpMessage(coapServerHost, coapServerPort, coapMessage);

    return 0;
}
