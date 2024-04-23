// #include <arpa/inet.h>
// #include <fcntl.h>
// #include <sqlite3.h>
// #include <sys/select.h>  // For select() and FD_SET
// #include <sys/socket.h>  // For socket, sendto, and close
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

struct Input {
    std::string name;
    std::vector<uint8_t> data;

    Input(const std::string& name, const std::string& value)
        : name(name), data(value.begin(), value.end()) {}
};

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
    sqlite3_finalize(stmt);
    int res1 = sqlite3_close(db);
    return 0;
}

std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}
std::string createHttpRequest(const std::vector<Input>& inputs) {
    std::string method = "GET"; // Default HTTP method
    std::string url = "/";      // Default URI
    std::map<std::string, std::string> headers;
    std::ostringstream body;

    for (const auto& input : inputs) {
        if (input.name == "method") {
            method = std::string(input.data.begin(), input.data.end());
        } else if (input.name == "url") {
            url = std::string(input.data.begin(), input.data.end());
        } else if (input.name == "Cookie") {
            headers["Cookie"] = "csrftoken=" + std::string(input.data.begin(), input.data.end());
        } else if (input.name == "Session") {
            headers["Session-ID"] = "sessionid=" + std::string(input.data.begin(), input.data.end());
        } else {
            // Handle body parameters
            if (!body.str().empty()) body << "&";
            body << input.name << "=" << urlEncode(std::string(input.data.begin(), input.data.end()));
        }
    }

    std::string bodyStr = body.str();

    std::ostringstream request;
    request << method << " " << uri << " HTTP/1.1\r\n";
    for (const auto& header : headers) {
        request << header.first << ": " << header.second << "\r\n";
    }
    if (!bodyStr.empty()) {
        request << "Content-Type: application/x-www-form-urlencoded\r\n";
        request << "Content-Length: " << bodyStr.length() << "\r\n";
    }
    request << "\r\n";
    request << bodyStr;

    return request.str();
}

int sendTcpMessageWithTimeout(const std::string& host, uint16_t port, const std::vector<uint8_t>& message) {
    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::string errorMessage = "Could not create TCP socket: ";
        errorMessage += strerror(errno);
        std::cerr << errorMessage << std::endl;
        return -1;
    }

    // Set the receive timeout option on the socket
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 Seconds timeout
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        std::cerr << "Set timeout failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }

    // Set up the server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }

    // Send the HTTP request
    if (send(sockfd, message.data(), message.size(), 0) < 0) {
        std::cerr << "Send failed: " << strerror(errno) << std::endl;
        close(sockfd);
        return -1;
    }

    // Receive response from the server
    std::string response;
    const int bufferSize = 4096;
    char buffer[bufferSize];
    ssize_t bytesRead = recv(sockfd, buffer, bufferSize, 0);
    if (bytesRead < 0) {
        // If no response is received before the timeout, recv will fail with EWOULDBLOCK/EAGAIN
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            std::cerr << "Receive timed out" << std::endl;
        } else {
            std::cerr << "Receive failed: " << strerror(errno) << std::endl;
        }
        close(sockfd);
        return -1;
    }

    // If some data was received, print it
    if (bytesRead > 0) {
        response.assign(buffer, bytesRead);
        std::cout << "Received response:\n" << response << std::endl;
    }

    // If no data received, it means the connection was closed by the server
    if (bytesRead == 0) {
        std::cerr << "Connection closed by server" << std::endl;
        close(sockfd);
        return -1;
    }

    // Close the socket
    close(sockfd);
    return 0; // Success
}
int run_driver(std::array<char, SIZE>& shm, std::vector<Input>& inputs) {
    // Define the CoAP server details.
    std::string coapServerHost = "127.0.0.1";
    uint16_t coapServerPort = 5683;

    // std::cout << inputVectorToJSON(inputs);

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
    std::vector<uint8_t> httpMessage = createHttpRequest(inputs);
    // Print each byte in hex format
    // for (uint8_t byte : coapMessage) {
    //     // Print byte in hex with leading zeros, formatted as width of 2
    //     std::cout << std::hex << std::setw(2) << std::setfill('0')
    //               << static_cast<int>(byte) << ' ';
    // }

    // std::cout << std::dec << std::setw(0) << std::setfill(' ');
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
pid_t runDjangoServer(const std::string& managePyPath, const std::string& ipAddress, const std::string& port) {
    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "Failed to fork" << std::endl;
        return -1; // return an error code
    } else if (pid == 0) {
        // Child process
        // Redirect stdout and stderr to /dev/null
        int devNull = open("/dev/null", O_WRONLY);
        if (devNull == -1 || dup2(devNull, STDOUT_FILENO) == -1 || dup2(devNull, STDERR_FILENO) == -1) {
            perror("dup2 or open");
            _exit(EXIT_FAILURE);
        }
        close(devNull); // Close the file descriptor as it is no longer needed

        // Run Django server
        char* args[] = {
            (char*)"python3",
            (char*)managePyPath.c_str(),
            (char*)"runserver",
            (char*)(ipAddress + ":" + port).c_str(),
            NULL
        };
        execvp(args[0], args);
        // If execvp returns, there was an error
        std::cerr << "Failed to execute Django server" << std::endl;
        _exit(EXIT_FAILURE); // Use _exit in child after fork
    }

    // Parent process
    return pid; // Return the child process ID
}
int main() {
    std::vector<Input> inputs = {
        Input("Method", "POST"),
        Input("URI", "/submit-data"),
        Input("info", "some information"),
        Input("name", "John Doe"),
        Input("price", 19.99), // Assuming price is a floating-point value
        Input("Cookie", "123456"),
        Input("SessionId", "abcdef")
    };

    std::string httpRequest = createHttpRequest(inputs);
    std::cout << "HTTP Request:\n" << httpRequest << std::endl;

    return 0;
}