The repo contains a sample server and a sample test driver which show the general expected flow and inputs / outputs from the test driver and server functions.

To run the test, run

```sh
make
```

in the root folder. It should compile the code in bin/fuzz_main.out.

Before running the test, make sure you have the coverage.py module available. If using `pipenv`, run

```sh
pipenv shell
```

Run the test with

```sh
./bin/fuzz_main.out
```

you should see output on the terminal and output files with the interesting inputs in the fuzz_out directory.