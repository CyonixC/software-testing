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