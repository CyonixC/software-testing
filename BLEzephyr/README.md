# TODO BEFORE EVERYTHING:
(Assuming you're in the BLEzephyr directory)

sudo apt install gcc-multilib gcovr lcov -y
tar -xf bumble.tar.gz # Extract bumble package
python3 -m pip install "./bumble" # Install bumble ble stack library
chmod +x zephyr.exe # Make target binary executable


# Test if you can run bumble and the server by first running:
python3 ori.py tcp-server:127.0.0.1:9000

# And in another terminal, run
GCOV_PREFIX=$(pwd) GCOV_PREFIX_STRIP=3 ./zephyr.exe --bt-dev=127.0.0.1:9000 