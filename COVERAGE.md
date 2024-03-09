Generate `.coverage` file with the `test_coverage.py` file.

```sh
python test_coverage.py
```

Compile `read_coverage.cpp`

```sh
g++ read_coverage.cpp sqlite3.o crc16.c -o covtest.out
```
Run it
```sh
./covtest.out
```
