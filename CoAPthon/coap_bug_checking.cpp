#include <arpa/inet.h>  // For inet_pton and htons
#include <fcntl.h>
#include <sqlite3.h>
#include <sys/select.h>  // For select() and FD_SET
#include <sys/socket.h>  // For socket, sendto, and close
#include <unistd.h>      // For close
#include <array>
#include <cerrno>   // For errno
#include <chrono>   // For system_clock
#include <cstddef>  // For std::byte
#include <cstdint>  // For uint8_t
#include <cstring>  // For memset
#include <cstring>  // For strerror
#include <iostream>
#include <stdexcept>  // Include for std::runtime_error
#include <string>
#include <vector>
#include "../checksum.h"
#include "../bug_checking.h"

// Function to construct the CoAP message using the Input vector.
std::vector<uint8_t> createCoapMessage(const std::vector<Input>& inputs) {
    uint8_t ver = 0x01;   // Default version is 1
    uint8_t type = 0x00;  // Default type is 0 (CON)
    uint8_t tkl = 0x00;   // Initialize TKL to zero
    uint8_t code = 0x00;  // Initialize Code to zero
    std::vector<uint8_t> messageId;
    std::vector<uint8_t> token;
    std::vector<uint8_t> uri_path;
    std::vector<uint8_t> options;
    std::vector<uint8_t> payload;

    // Parse inputs to collect CoAP fields
    for (const auto& input : inputs) {
        if (input.name == "Type") {
            type = std::to_integer<uint8_t>(input.data[0]);
        } else if (input.name == "TKL") {
            tkl = std::to_integer<uint8_t>(input.data[0]);
        } else if (input.name == "Code") {
            code = std::to_integer<uint8_t>(input.data[0]);
        } else if (input.name == "MessageID") {
            for (auto& b : input.data) {
                messageId.push_back(std::to_integer<uint8_t>(b));
            }
        } else if (input.name == "Token") {
            for (auto& b : input.data) {
                token.push_back(std::to_integer<uint8_t>(b));
            }
        } else if (input.name == "Uri-Path") {
            // Need to check the size of the Uri-Path to determine if it needs to be extended or not
            if (input.data.size() > 12) {
                // if the Uri-Path is longer than 12 bytes, the option header needs to be extended
                uri_path.push_back(
                    0xBD);  // "B" to mark the URI-Path option, and "D" to indicate the extension
                uri_path.push_back(
                    input.data.size() -
                    13);  // The length of the extended URI-Path (minus the first 13 bytes)
            } else {
                // No extension needed, construct the normal header
                uri_path.push_back((
                    0xB0 +
                    input.data
                        .size()));  // "B" to mark the URI-Path option, and the last 4 bits is the length
            }

            for (auto& b : input.data) {
                uri_path.push_back(std::to_integer<uint8_t>(b));
            }
        } else if (input.name == "Options") {
            // Here we would need to implement logic to correctly serialize the options
        } else if (input.name == "Payload") {
            // Payload is preceded by a marker if there is a payload and options are present
            if (!input.data.empty()) {
                payload.push_back(0xFF);  // Payload marker
                for (auto& b : input.data) {
                    payload.push_back(std::to_integer<uint8_t>(b));
                }
            }
        }
    }

    // Ensure TKL matches the token length
    tkl = static_cast<uint8_t>(
        token
            .size());  // Ensure this happens after the last modification to `token`

    // Construct the message header
    uint8_t header = (ver << 6) | (type << 4) | tkl;

    // Construct the complete CoAP message
    std::vector<uint8_t> message;
    message.push_back(header);
    message.push_back(code);
    message.insert(message.end(), messageId.begin(), messageId.end());
    message.insert(message.end(), token.begin(), token.end());
    message.insert(message.end(), uri_path.begin(), uri_path.end());
    message.insert(message.end(), payload.begin(), payload.end());
    return message;
}

int sendUdpMessage(const std::string& host, uint16_t port,
                   const std::vector<uint8_t>& message) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::string errorMessage = "Could not create UDP socket: ";
        errorMessage += strerror(errno);
        std::cerr << errorMessage << std::endl;
        throw std::runtime_error(errorMessage);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));  // Corrected line
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported: "
                  << strerror(errno) << std::endl;
        close(sockfd);
        throw std::runtime_error("Invalid address/Address not supported");
    }

    if (sendto(sockfd, message.data(), message.size(), 0,
               (const struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        std::string errorMessage = "Could not send message: ";
        errorMessage += strerror(errno);
        std::cerr << errorMessage << std::endl;
        close(sockfd);
        throw std::runtime_error(errorMessage);
    }

    // Setting up for receiving with timeout
    struct timeval tv;
    tv.tv_sec = 1;  // 10 Seconds timeout
    tv.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Wait for the response with a timeout
    int retval = select(sockfd + 1, &readfds, nullptr, nullptr, &tv);
    if (retval == -1) {
        std::cerr << "Select error: " << strerror(errno) << std::endl;
    } else if (retval) {
        char buffer[1024];
        struct sockaddr_in src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                               (struct sockaddr*)&src_addr, &src_addr_len);
        if (len == -1) {
            std::cerr << "Receive error: " << strerror(errno) << std::endl;
        }

    } else {
        close(sockfd);
        return 1;
    }

    if (close(sockfd) == -1) {
        std::cerr << "Could not close socket: " << strerror(errno) << std::endl;
        return 0;
    }
    return 0;
}

int run_driver(std::vector<Input>& inputs) {
    std::string coapServerHost = "127.0.0.1";
    uint16_t coapServerPort = 5683;

    std::vector<uint8_t> coapMessage = createCoapMessage(inputs);
    int result = sendUdpMessage(coapServerHost, coapServerPort, coapMessage);

    if (result == 1) {
        std::cout << "Timeout occurred or no response received." << std::endl;
        return 1;
    }
    return 0;
}

pid_t run_server() {
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "Failed to fork" << std::endl;
        return 0;
    } else if (pid == 0) {
        // Child process

        // Constructing the command with sudo. This assumes the user has passwordless sudo set up for gdb.

        char* args[] = {
            (char*)"sudo",
            (char*)"gdb",
            (char*)"-ex",
            (char*)"run",
            (char*)"-ex",
            (char*)"backtrace",
            (char*)"--args",
            (char*)"python2",
            (char*)"CoAPthon/coapserver.py",
            (char*)"-i",
            (char*)"127.0.0.1",
            (char*)"-p",
            (char*)"5683",
            NULL};
        execvp(args[0], args);
        // If execvp returns, an error occurred
        std::cerr << "Failed to execute command" << std::endl;
        _exit(EXIT_FAILURE);  // Use _exit in child after fork

    }
    return pid;
}