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
#include "../driver.h"

// Input structure containing data and its associated name.

int hash_cov_into_shm(std::array<char, SIZE>& shm, const char* filename) {
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;
    rc = sqlite3_open(filename, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return (0);
    } else {
        // fprintf(stderr, "Opened database successfully\n");
    }

    const char* sql = "SELECT * from arc";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    int ncols = sqlite3_column_count(stmt);
    int res;
    do {
        res = sqlite3_step(stmt);
        uint16_t crc = 0;
        for (int i = 0; i < ncols; i++) {
            int item = sqlite3_column_int(stmt, i);
            crc = update_crc_16(crc, item);
        }
        shm[crc]++;
    } while (res == SQLITE_ROW);
    sqlite3_close(db);
    return 0;
}

// Function to construct the CoAP message using the Input vector.
std::vector<uint8_t> createCoapMessage(const std::vector<Input>& inputs) {
    // Placeholder for header fields
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
    std::cout << "Current Token Length: " << token.size() << std::endl;

    // Construct the message header
    uint8_t header = (ver << 6) | (type << 4) | tkl;

    // Construct the complete CoAP message
    std::vector<uint8_t> message;
    message.push_back(header);
    std::cout << std::hex << std::setw(2) << std::setfill('0');
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;

    message.push_back(code);
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;

    message.insert(message.end(), messageId.begin(), messageId.end());
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;

    message.insert(message.end(), token.begin(), token.end());
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;
    // Options would be added here, after implementing options serialization logic

    message.insert(message.end(), uri_path.begin(), uri_path.end());
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;

    message.insert(message.end(), payload.begin(), payload.end());
    for (uint8_t byte : message) {
        std::cout << static_cast<int>(byte) << ' ';
    }
    std::cout << std::endl;
    std::cout << std::dec << std::setw(0) << std::setfill(' ');

    return message;
}

// Function to send the message over UDP.
int sendUdpMessage(const std::string& host, uint16_t port,
                   const std::vector<uint8_t>& message,
                   std::array<char, SIZE>& shm) {
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
    std::cout << "Message sent successfully to " << host << ":" << port
              << std::endl;

    // Setting up for receiving with timeout
    struct timeval tv;
    tv.tv_sec = 10;  // 10 Seconds timeout
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
        } else {
            std::cout << "Received response: " << std::string(buffer, len)
                      << std::endl;
        }
    } else {
        return 1;
    }

    close(sockfd);
    return 0;
}
int run_driver(std::array<char, SIZE>& shm, std::vector<Input>& inputs) {
    // Define the CoAP server details.
    std::string coapServerHost = "127.0.0.1";
    uint16_t coapServerPort = 5683;

    std::cout << inputVectorToJSON(inputs);

    // // Define the example inputs for the various parts of the CoAP message.
    // std::vector<Input> inputs = {
    //     {std::vector<std::byte>{std::byte(0x01)}, "Version"},                                                                                                 // CoAP version (01)
    //     {std::vector<std::byte>{std::byte(0x00)}, "Type"},                                                                                                    // Type (Confirmable: 0)
    //     {std::vector<std::byte>{std::byte(0x01)}, "Code"},                                                                                                    // Code: GET (0.01)
    //     {std::vector<std::byte>{std::byte(0xC4), std::byte(0x09)}, "MessageID"},                                                                              // Message ID (0xC409)
    //     {std::vector<std::byte>{std::byte(0x74), std::byte(0x65), std::byte(0x73), std::byte(0x74)}, "Token"},                                                // Token ('test')
    //     {std::vector<std::byte>{std::byte('e'), std::byte('x'), std::byte('a'), std::byte('m'), std::byte('p'), std::byte('l'), std::byte('e')}, "Uri-Path"}, // Uri-Path: 'example'
    //     //{std::vector<std::byte>{std::byte(0xC4), std::byte(0x19)}, "Payload"} // Message ID (0xC409)
    // };

    // Create the CoAP message.
    std::vector<uint8_t> coapMessage = createCoapMessage(inputs);
    // Print each byte in hex format
    for (uint8_t byte : coapMessage) {
        // Print byte in hex with leading zeros, formatted as width of 2
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << ' ';
    }

    std::cout << std::dec << std::endl;  // Reset std::cout to decimal
    // Send the CoAP message over UDP and handle the response.
    int result =
        sendUdpMessage(coapServerHost, coapServerPort, coapMessage, shm);
    // hash here?
    hash_cov_into_shm(shm, "data/.coverage");

    // Handle the result as needed
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

        // Redirect stdout to /dev/null
        if (dup2(open("/dev/null", O_WRONLY), STDOUT_FILENO) == -1) {
            perror("dup2");
            return 1;
        }

        // Constructing the command with sudo. This assumes the user has passwordless sudo set up for gdb.

        char* args[] = {
            (char*)"sudo",
            (char*)"gdb",
            (char*)"-ex",
            (char*)"run",
            (char*)"-ex",
            (char*)"backtrace",
            (char*)"--args",
            (char*)"/home/professor_a/.pyenv/versions/2.7.18/bin/python",
            (char*)"CoAPthon/coapserver.py",
            (char*)"-i",
            (char*)"127.0.0.1",
            (char*)"-p",
            (char*)"5683",
            (char*)">",
            (char*)"/dev/null",
            (char*)"2>&1",
            NULL};
        execvp(args[0], args);
        // If execvp returns, an error occurred
        std::cerr << "Failed to execute command" << std::endl;
        _exit(EXIT_FAILURE);  // Use _exit in child after fork

        // int status = std::system(
        //     "sudo gdb -ex run -ex backtrace --args "
        //     "/home/professor_a/.pyenv/versions/2.7.18/bin/python "
        //     "CoAPthon/coapserver.py -i 127.0.0.1 -p 5683 > /dev/null 2>&1");

        // if (status != 0) {
        //     // std::cerr << "Error executing server: " << std::endl;
        //     exit(1);
        // }
        // exit(0);  // Exit the child process
    }
    // Parent process
    return pid;
}