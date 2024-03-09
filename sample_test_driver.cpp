#include <sqlite3.h>
#include <stdio.h>
#include "checksum.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int read_coverage(char* shm);
int run_coverage_shm(char* shm) {
    // Define the server's IP address and port
    const char* SERVER_IP = "127.0.0.1";
    const int SERVER_PORT = 4345;

    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error: Could not create socket\n";
        return 1;
    }

    // Define the server address structure
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Error: Connection failed\n";
        close(client_socket);
        return 1;
    }
    std::cout << "Connected to the server.\n";

    // Send data to the server
    const char* message = "Ping";
    if (send(client_socket, message, strlen(message), 0) == -1) {
        std::cerr << "Error: Failed to send data\n";
        close(client_socket);
        return 1;
    }
    std::cout << "Sent: " << message << std::endl;

    // Receive data from the server
    char buffer[1024];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received == -1) {
        std::cerr << "Error: Failed to receive data\n";
        close(client_socket);
        return 1;
    }
    buffer[bytes_received] = '\0';
    std::cout << "Received: " << buffer << std::endl;

    // Close the socket
    close(client_socket);

    read_coverage(shm);

    return 0;
}

int read_coverage(char* shm) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    rc = sqlite3_open("data/.coverage", &db);
    if( rc ) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return(0);
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }

    const char *sql = "SELECT * from arc";
    sqlite3_stmt *stmt; 
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
        // printf("%04x\n", crc);
        printf("Data at index %d: %d\n", crc, shm[crc]);
        shm[crc]++;
    } while (res == SQLITE_ROW);
    sqlite3_close(db);
    return 0;
}

