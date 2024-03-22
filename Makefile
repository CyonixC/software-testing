OUTPUT_FOLDER = bin/

main_fuzz: fuzz_main.cpp crc16.c sample_test_driver.cpp read_files.cpp
	g++ fuzz_main.cpp sqlite3.o crc16.c sample_test_driver.cpp read_files.cpp -o ${OUTPUT_FOLDER}fuzz_main.out -g

config: config.cpp inputs.cpp
	g++ config.cpp inputs.cpp -o ${OUTPUT_FOLDER}read_files.out

config_debug: config.cpp inputs.cpp
	g++ config.cpp inputs.cpp -o ${OUTPUT_FOLDER}read_files.out -g