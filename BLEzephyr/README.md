*TODO BEFORE EVERYTHING:*

```shell
# (in BLEzephyr folder)
sudo apt install gcc-multilib gcovr lcov -y
tar -xf bumble.tar.gz # Extract bumble package
python3 -m pip install "./bumble" # Install bumble ble stack library
chmod +x zephyr.exe # Make target binary executable
```


Test if you can run bumble and the server by first running:

```shell
python3 ori.py tcp-server:127.0.0.1:9000
```

And in another terminal, run

```shell
GCOV_PREFIX=$(pwd) GCOV_PREFIX_STRIP=3 ./zephyr.exe --bt-dev=127.0.0.1:9000
```

This causes a bug. Python should complain of a read error, and zephyr should be stuck in a buffer overflow infinite loop