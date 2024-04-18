#include <iostream>
#include <vector>
#include <cstddef> // For std::byte
#include <cstdint> // For uint8_t
#include <string>
#include <arpa/inet.h>  // For inet_pton and htons
#include <sys/socket.h> // For socket, sendto, and close
#include <unistd.h>     // For close
#include <cstring>      // For memset
#include <sys/select.h> // For select() and FD_SET
#include <chrono>       // For system_clock
#include <cstring>      // For strerror
#include <cerrno>       // For errno
#include <array>
#include <sqlite3.h>
#include "../driver.h"
#include "../checksum.h"
#include <stdexcept> // Include for std::runtime_error

// Input structure containing data and its associated name.

int hash_cov_into_shm(std::array<char, SIZE> &shm, const char *filename)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    rc = sqlite3_open(filename, &db);
    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return (0);
    }
    else
    {
        // fprintf(stderr, "Opened database successfully\n");
    }

    const char *sql = "SELECT * from arc";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    int ncols = sqlite3_column_count(stmt);
    int res;
    do
    {
        res = sqlite3_step(stmt);
        uint16_t crc = 0;
        for (int i = 0; i < ncols; i++)
        {
            int item = sqlite3_column_int(stmt, i);
            crc = update_crc_16(crc, item);
        }
        shm[crc]++;
    } while (res == SQLITE_ROW);
    sqlite3_close(db);
    return 0;
}

// Function to construct the CoAP message using the Input vector.
std::vector<uint8_t> createCoapMessage(const std::vector<Input> &inputs)
{
    std::vector<uint8_t> message;
    // The first byte of the CoAP header includes version, type, and token length (TKL).
    uint8_t header = 0x00; // Will be built up from the inputs.

    for (const auto &input : inputs)
    {
        if (input.name == "Version")
        {
            header |= (std::to_integer<uint8_t>(input.data[0]) << 6);
        }
        else if (input.name == "Type")
        {
            header |= (std::to_integer<uint8_t>(input.data[0]) << 4);
        }
        else if (input.name == "TKL")
        {
            header |= std::to_integer<uint8_t>(input.data[0]);
        }
        else if (input.name == "Code")
        {
            message.push_back(std::to_integer<uint8_t>(input.data[0]));
        }
        else if (input.name == "MessageID")
        {
            for (auto &b : input.data)
            {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        }
        else if (input.name == "Token")
        {
            tkl = input.data.size();
            for (auto &b : input.data)
            {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        }
        else if (input.name == "Uri-Path")
        {
            // Assuming Uri-Path is a single option and input data does not include option header.
            message.push_back(0xB0 | 0x0B);       // Option delta for Uri-Path
            message.push_back(input.data.size()); // Option length
            for (auto &b : input.data)
            {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        }
        else if (input.name == "Payload")
        {
            for (auto &b : input.data)
            {
                message.push_back(std::to_integer<uint8_t>(b));
            }
        }
        // Include additional cases as needed for other CoAP message components.
    }

    // Prepend the built header to the message.
    header |= tkl;
    message.insert(message.begin(), header);
    cout <<< message;
    return message;
}

// Function to send the message over UDP.
int sendUdpMessage(const std::string &host, uint16_t port, const std::vector<uint8_t> &message, std::array<char, SIZE> &shm)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        std::string errorMessage = "Could not create UDP socket: ";
        errorMessage += strerror(errno);
        std::cerr << errorMessage << std::endl;
        throw std::runtime_error(errorMessage);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); // Corrected line
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &servaddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address/Address not supported: " << strerror(errno) << std::endl;
        close(sockfd);
        throw std::runtime_error("Invalid address/Address not supported");
    }

    if (sendto(sockfd, message.data(), message.size(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        std::string errorMessage = "Could not send message: ";
        errorMessage += strerror(errno);
        std::cerr << errorMessage << std::endl;
        close(sockfd);
        throw std::runtime_error(errorMessage);
    }
    std::cout << "Message sent successfully to " << host << ":" << port << std::endl;

    // Setting up for receiving with timeout
    struct timeval tv;
    tv.tv_sec = 10; // 10 Seconds timeout
    tv.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Wait for the response with a timeout
    int retval = select(sockfd + 1, &readfds, nullptr, nullptr, &tv);
    if (retval == -1)
    {
        std::cerr << "Select error: " << strerror(errno) << std::endl;
    }
    else if (retval)
    {
        char buffer[1024];
        struct sockaddr_in src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        ssize_t len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr, &src_addr_len);
        if (len == -1)
        {
            std::cerr << "Receive error: " << strerror(errno) << std::endl;
        }
        else
        {
            std::cout << "Received response: " << std::string(buffer, len) << std::endl;
            hash_cov_into_shm(shm, "data/.coverage");
        }
    }
    else
    {
        return 1;
    }

    close(sockfd);
    return 0;
}
int run_driver(std::array<char, SIZE> &shm, std::vector<Input>& inputs) {
    // Define the CoAP server details.
    std::string coapServerHost = "127.0.0.1";
    uint16_t coapServerPort = 5683;

    std::cout << inputVectorToJSON(inputs);

    // Define the example inputs for the various parts of the CoAP message.
    std::vector<Input> inputs = {
        {std::vector<std::byte>{std::byte(0x01)}, "Version"},                                                                                                 // CoAP version (01)
        {std::vector<std::byte>{std::byte(0x00)}, "Type"},                                                                                                    // Type (Confirmable: 0)
        {std::vector<std::byte>{std::byte(0x01)}, "Code"},                                                                                                    // Code: GET (0.01)
        {std::vector<std::byte>{std::byte(0xC4), std::byte(0x09)}, "MessageID"},                                                                              // Message ID (0xC409)
        {std::vector<std::byte>{std::byte(0x74), std::byte(0x65), std::byte(0x73), std::byte(0x74)}, "Token"},                                                // Token ('test')
        {std::vector<std::byte>{std::byte('e'), std::byte('x'), std::byte('a'), std::byte('m'), std::byte('p'), std::byte('l'), std::byte('e')}, "Uri-Path"}, // Uri-Path: 'example'
        //{std::vector<std::byte>{std::byte(0xC4), std::byte(0x19)}, "Payload"} // Message ID (0xC409)
    };

    // Create the CoAP message.
    std::vector<uint8_t> coapMessage = createCoapMessage(inputs);

    // Send the CoAP message over UDP and handle the response.
    int result = sendUdpMessage(coapServerHost, coapServerPort, coapMessage, shm);

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
            (char*)"coapserver.py",
            (char*)"-i",
            (char*)"127.0.0.1",
            (char*)"-p",
            (char*)"5683",
            NULL
        };
        execvp(args[0], args);
        // If execvp returns, an error occurred
        std::cerr << "Failed to execute command" << std::endl;
        _exit(EXIT_FAILURE); // Use _exit in child after fork
    }
    // Parent process
    return pid;
}