#include <stdio.h>
#include <iostream>
#include <filesystem>
#include <unistd.h> // For fork(), execvp()
#include <sys/wait.h> // For waitpid()
#include <iostream>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include "sample_test_driver.h"
#include <cstring>

using namespace std;

pid_t run_py();
int run_driver(char* shm);

int main() {
    // Shared memory key
    key_t key = 654;
    const int SIZE = 65536;

    // Create shared memory segment
    int shmid = shmget(key, SIZE, 0666|IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    // Attach shared memory segment to parent process
    char* data = (char*) shmat(shmid, NULL, 0);
    if (data == (char *)(-1)) {
        perror("shmat");
        return 1;
    }

    // Zero out the shared memory segment
    memset(data, 0, SIZE);

    // Run the coverage Python script to generate the .coverage file
    // This is a stand-in for the actual server, and is just listening for connections on 4345.
    pid_t pid = run_py();
    printf("Python server started\n");
    sleep(1); // Wait for the server to start, on actual should probably use a signal or something

    for (int i = 0; i < 10; i++) {
        // Run the driver a few times
        run_driver(data);
        for (int j = 0; j < SIZE; j++) {
            if (data[j] != 0) {
                printf("Data at index %d: %d\n", j, data[j]);
            }
        }
        // Zero out the shared memory segment
        memset(data, 0, SIZE);

    }
    // Detach shared memory segment from parent process
    if (shmdt(data) == -1) {
        perror("shmdt");
        return 1;
    }
    
    // Mark shared memory segment for deletion
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return 1;
    }

    kill(pid, SIGTERM); // Kill the Python server
    

}

int run_driver(char* shm) {

    pid_t pid = fork();

    if (pid == -1) {
        // Error occurred
        std::cerr << "Failed to fork" << std::endl;
        return 1;
    } else if (pid == 0) {
        // Child process

        // Run the driver
        run_coverage_shm(shm);
        if (shmdt(shm) == -1) {
            perror("shmdt");
            return 1;
        }
    } else {
        // Parent process
        // Wait for the child process to finish
        waitpid(pid, NULL, 0);
        std::cout << "Driver finished" << std::endl;
    }
    return 0;

}

pid_t run_py() {

    pid_t pid = fork();

    if (pid == -1) {
        // Error occurred
        std::cerr << "Failed to fork" << std::endl;
        return 0;
    } else if (pid == 0) {
        // Child process
        char* args[] = {(char*)"python", (char*)"test_coverage.py", NULL};
        execvp(args[0], args);
        // If execvp returns, an error occurred
        std::cerr << "Failed to execute Python script" << std::endl;
        return 0;
    } else {
        // Parent process
        // Wait for the child process to finish
    }
    return pid;
}
