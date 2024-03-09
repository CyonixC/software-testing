OUTPUT_FOLDER = bin/

main_fuzz: fuzz_main.cpp crc16.c sample_test_driver.cpp
	g++ fuzz_main.cpp sqlite3.o crc16.c sample_test_driver.cpp -o ${OUTPUT_FOLDER}fuzz_main.out
