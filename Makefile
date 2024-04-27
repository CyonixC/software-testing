OUTPUT_FOLDER = bin

ifdef ASAN
	SANITIZER_FLAG = -fsanitize=address -static-libasan
	DEBUG = 1
endif

ifdef DEBUG
	DEBUG_FLAG = -g -DDEBUG
endif

coap: fuzz_main.cpp inputs.cpp crc16.c config.cpp CoAPthon/coap_test_driver.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c CoAPthon/coap_test_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/coap.json" -DPROGRAM_NAME="coap"

ble: fuzz_main.cpp inputs.cpp crc16.c config.cpp BLEzephyr/ble_driver.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp crc16.c BLEzephyr/ble_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/ble.json" -DPROGRAM_NAME="ble"

django: fuzz_main.cpp inputs.cpp crc16.c config.cpp DjangoWebApplication/django_test_driver.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c DjangoWebApplication/django_test_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/django.json" -DPROGRAM_NAME="django"

coap_bug_checker: bug_tester.cpp inputs.cpp config.cpp CoAPthon/coap_bug_checking.cpp $(OUTPUT_FOLDER)
	g++ bug_tester.cpp inputs.cpp CoAPthon/coap_bug_checking.cpp config.cpp -o ${OUTPUT_FOLDER}/bug_checker.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/coap.json"

ble_bug_checker: bug_tester.cpp inputs.cpp config.cpp CoAPthon/coap_test_driver.cpp $(OUTPUT_FOLDER)
	g++ bug_tester.cpp inputs.cpp CoAPthon/coap_test_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/bug_checker.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/ble.json"

django_bug_checker: bug_tester.cpp inputs.cpp config.cpp CoAPthon/coap_test_driver.cpp $(OUTPUT_FOLDER)
	g++ bug_tester.cpp inputs.cpp CoAPthon/coap_test_driver.cpp config.cpp -o ${OUTPUT_FOLDER}/bug_checker.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/django.json"

sample: fuzz_main.cpp inputs.cpp crc16.c sample_program.cpp config.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c sample_program.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG) $(SANITIZER_FLAG) -DCONFIG_FILE="configs/input_config_example.json"

$(OUTPUT_FOLDER):
	mkdir $(OUTPUT_FOLDER)