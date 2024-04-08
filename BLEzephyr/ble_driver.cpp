#include "ble_driver.h"

#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <sys/wait.h>

void write_data(const std::vector<std::byte> &bytes_vec, const int wfd)
{
    int byte_len = bytes_vec.size();
    // Convert the integer to an array of bytes
    uint8_t bytes[2];
    bytes[0] = static_cast<uint8_t>(byte_len & 0xFF);        // Least significant byte
    bytes[1] = static_cast<uint8_t>((byte_len >> 8) & 0xFF); // Second byte

    if (write(wfd, bytes, 2) == -1)
    {
        std::cerr << "Error writing number of bytes" << std::endl;
    }
    if (write(wfd, bytes_vec.data(), bytes_vec.size()) == -1)
    {
        std::cerr << "Error writing : actual string" << std::endl;
    }
}

std::string read_data(const int rfd)
{
    uint8_t len_buffer[2];
    if (read(rfd, len_buffer, 2) == -1)
    {
        std::cerr << "Error reading from pipe: Buffer length" << std::endl;
    }

    uint32_t length = static_cast<uint32_t>(len_buffer[1]) * 8 + static_cast<uint32_t>(len_buffer[0]);
    char buffer[1024];
    if (read(rfd, buffer, length) == -1)
    {
        std::cerr << "Error reading from pipe: Actual message of length " << length << std::endl;
    }

    return std::string{buffer, length};
}

void try_create_fifo(const std::string &cpp_fifo_name, const std::string &python_fifo_name)
{
    if (mkfifo(cpp_fifo_name.data(), S_IRWXU) == -1)
    {
        perror("Error creating cpp.fifo");
    }
    if (mkfifo(python_fifo_name.data(), 0666) == -1)
    {
        perror("Error creating python.fifo");
    }
}

void run_zephyr_server()
{
    int status = std::system("GCOV_PREFIX=$(pwd) GCOV_PREFIX_STRIP=3 ./zephyr.exe --bt-dev=127.0.0.1:9000 ");

    if (status != 0)
    {
        std::cerr << "Error executing server: " << std::endl;
        exit(1);
    }
    exit(0); // Exit the child process
}

void run_python_ble_tester()
{
    int status = std::system("python3 run_ble_tester.py tcp-server:127.0.0.1:9000");

    if (status != 0)
    {
        std::cerr << "Error executing python: " << std::endl;
        exit(1);
    }
    exit(0); // Exit the child process
}

void send_inputs_to_python(const int wfd, const int rfd, const std::vector<Input> &inputs)
{
    std::cout << "Received from python: " << read_data(rfd) << std::endl;
    int msg_count = inputs.size();
    const std::vector<std::byte> msg_count_vec{static_cast<std::byte>(msg_count & 0xFF), static_cast<std::byte>((msg_count >> 8) & 0xFF)};
    write_data(msg_count_vec, wfd);

    int i = 1;
    for (auto &input : inputs)
    {
        std::cout << "Received from python: " << read_data(rfd) << std::endl;
        write_data(input.data, wfd);
        std::cout << "Send to python: " << i << "th message" << std::endl;
        i++;
    }
}

void wait_python_exit(const int pid)
{
    int childStatus;
    waitpid(pid, &childStatus, 0); // Wait for the child process to finish
    if (WIFEXITED(childStatus))
    {
        std::cout << "Child process exited with status " << WEXITSTATUS(childStatus) << std::endl;
    }
    else if (WIFSIGNALED(childStatus))
    {
        std::cerr << "Child process terminated due to signal " << WTERMSIG(childStatus) << std::endl;
    }
}

void shutdown_zephyr_server(const int pid)
{
    int status;
    auto result = waitpid(pid, &status, WNOHANG);

    if (result == -1)
    {
        std::cerr << "Error checking process status" << std::endl;
        return;
    }

    if (result != 0)
    {
        // Process has exited it probably crashed.
        return;
    }

    // Zephyr is still running. Kill it.
    std::cout << "Main driver: killing the zephyr server..." << std::endl;
    status = std::system("pkill -15 -f './zephyr.exe'");
    return;
}

void get_coverage_data()
{
    int status = std::system("lcov --capture --directory ./ --output-file lcov.info -q --rc lcov_branch_coverage=1");

    if (status != 0)
    {
        std::cerr << "Error getting coverage data: " << std::endl;
    }

    std::cout << "Main driver: Sucessfully gotten coverage data. Killing zephyr again?" << std::endl;
    status = std::system("pkill -15 -f './zephyr.exe'");
    return;
}

int driver(std::vector<Input> &inputs)
{
    std::string cpp_fifo_name = "./pipe/cpp.fifo";
    std::string python_fifo_name = "./pipe/python.fifo";
    // try_create_fifo(cpp_fifo_name, python_fifo_name);

    auto pid_1 = fork(); // First fork: BLE python
    if (pid_1 == 0)
    {
        run_python_ble_tester();
    }
    else if (pid_1 < 0)
    {
        std::cerr << "Fork1 failed!" << std::endl;
        return 1;
    }

    // Connect with python pipe. Zephyr will be ready when python connected with the pipe
    auto wfd = open(python_fifo_name.data(), O_WRONLY);
    auto rfd = open(cpp_fifo_name.data(), O_RDONLY);

    auto pid_2 = fork(); // Second fork: Zephyr server
    if (pid_2 == 0)
    {
        run_zephyr_server();
    }
    else if (pid_2 < 0)
    {
        std::cerr << "Fork2 failed!" << std::endl;
        return 1;
    }

    send_inputs_to_python(wfd, rfd, inputs);
    wait_python_exit(pid_1);
    close(wfd);
    close(rfd);
    shutdown_zephyr_server(pid_2);
    get_coverage_data();

    return 0;
}

int main()
{
    std::vector<Input> inputs{};
    inputs.push_back(Input{std::vector<std::byte>{std::byte{0x06}}, ""});
    inputs.push_back(Input{std::vector<std::byte>{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}}, ""});
    inputs.push_back(Input{std::vector<std::byte>{std::byte{0x10}, std::byte{0x11}, std::byte{0x12}}, ""});
    driver(inputs);
}