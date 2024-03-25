The important part for the test drivers is located in `sample_program.h` and `sample_program.cpp`. When the fuzzer is run, the following functions are called to launch the server, feed it inputs and collect coverage:

1. **run_server()**, which forks a process to start the server and returns the *pid* of the child process.
2. **run_driver()**, which takes a `std::array` of 65536 bytes, and a single `InputSeed`. The `std::array` is used to track coverage.

## Program Flow

The flow is as follows:
1. Fuzzer launches server with `run_server()`
2. Fuzzer waits for server to be up
    - If possible, add a signal to the startup process in the function
3. Fuzzer sends the input to the driver
4. Driver assembles the input and sends it to the server
5. Driver either:
    5a. Receives a response from the server
    5b. Times out while waiting for the response
6. In both cases, driver will measure coverage, and hash it into the 65536 byte `coverage_array`.
7a. Driver returns with a success code (0) if the server responded, or
7b. Driver returns with a failure code (1) if the server crashed
8. Fuzzer takes the `coverage_array` and evaluates the current input
9. If is interesting, add to queue.
10. Fuzzer mutates the seed and starts the driver again from step 3

## Coverage measurement
If using `coverage.py`, I've written some C++ code to read from the `.coverage` database file and hash the result into the `coverage_array`.

If not, the idea is basically:
1. Run CRC16 on each individual branch transition (basically hash it)
2. Use the hash as an index, and increment the corresponding byte in the array