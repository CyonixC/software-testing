OUTPUT_FOLDER = bin

ifdef DEBUG
	DEBUG_FLAG = -g -DDEBUG
endif

main_fuzz: fuzz_main.cpp inputs.cpp crc16.c sample_program.cpp config.cpp $(OUTPUT_FOLDER)
	g++ fuzz_main.cpp inputs.cpp sqlite3.o crc16.c sample_program.cpp config.cpp -o ${OUTPUT_FOLDER}/fuzz_main.out $(DEBUG_FLAG)

$(OUTPUT_FOLDER):
	mkdir $(OUTPUT_FOLDER)