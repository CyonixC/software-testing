#include <stdio.h>
#include <stdint.h>
#include "checksum.h"

int main() {
    const char *str = "Hello World";
    uint16_t crc = 0;
    for (const char *p = str; *p; p++) {
        crc = update_crc_16(crc, *p);
        printf("%04x\n", crc);
    }
}