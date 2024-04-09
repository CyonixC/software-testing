OUTPUT_FOLDER = bin

ifdef DEBUG
	DEBUG_FLAG = -g -DDEBUG
endif

sample: fuzz_main.cpp inputs.cpp crc16.c sample_program.cpp config.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c sample_program.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG)

coap: fuzz_main.cpp inputs.cpp crc16.c config.cpp CoAPthon/coap_test_driver.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c CoAPthon/coap_test_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG)

ble: fuzz_main.cpp inputs.cpp crc16.c config.cpp BLEzephyr/ble_driver.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c BLEzephyr/ble_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG)

$(OUTPUT_FOLDER):
	mkdir $(OUTPUT_FOLDER)