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
#include "inputs.h"
#include <random>
#include <iostream>

template<typename T>
T random_int(T min, T max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<T> dis(min, max);
    return dis(gen);
}

using namespace std;

pid_t run_py();
int run_driver(char* shm);
const int SIZE = 65536;
key_t key = 654;

int main() {
    // Shared memory key

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

bool isInteresting(char* data) {
    // This is a mirror of the array produced by the coverage tool
    // to track which branches have been taken

    // Each char element contains the bucket count of the number of times
    // a branch has been taken.
    static char* tracking = new char[SIZE]();

    // Bucketing branch transition counts
    bool is_interesting = false;
    for (int i = 0; i < SIZE; i++) {
        if (data[i] >= 128 && tracking[i] >> 7 == 0) {
            tracking[i] |= 0b10000000;
            is_interesting = true;
        } else if (data[i] >= 32 && (tracking[i] >> 6) % 2 == 0) {
            tracking[i] |= 0b01000000;
            is_interesting = true;
        } else if (data[i] >= 16 && (tracking[i] >> 5) % 2 == 0) {
            tracking[i] |= 0b00100000;
            is_interesting = true;
        } else if (data[i] >= 8 && (tracking[i] >> 4) % 2 == 0) {
            tracking[i] |= 0b00010000;
            is_interesting = true;
        } else if (data[i] >= 4 && (tracking[i] >> 3) % 2 == 0) {
            tracking[i] |= 0b00001000;
            is_interesting = true;
        } else if (data[i] == 3 && (tracking[i] >> 2) % 2 == 0) {
            tracking[i] |= 0b00000100;
            is_interesting = true;
        } else if (data[i] == 2 && (tracking[i] >> 1) % 2 == 0) {
            tracking[i] |= 0b00000010;
            is_interesting = true;
        } else if (data[i] == 1 && tracking[i] % 2 == 0) {
            tracking[i] |= 0b00000001;
            is_interesting = true;
        }
    }

    return is_interesting;
}

void assignEnergy(Input& input) {
    // Just assign constant energy
    const int ENERGY = 100;
    input.energy = ENERGY;
}
void mutate(Input& input) {
    if (input.data.empty()) return; // Check if there's data to mutate

    // Decide how many bytes to mutate based on the size of the data
    size_t mutationsCount = random_int<size_t>(1, input.data.size() / 2);

    for (size_t i = 0; i < mutationsCount; ++i) {
        // Pick a random byte to mutate
        size_t index = random_int<size_t>(0, input.data.size() - 1);

        // Generate a new random byte and replace the chosen byte
        std::byte newValue = static_cast<std::byte>(random_int<int>(0, 255));
        input.data[index] = newValue;
    }

    std::cout << "Mutated " << mutationsCount << " bytes." << std::endl;
}