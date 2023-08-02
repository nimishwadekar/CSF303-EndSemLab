#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint8_t KEY[] = "Bo-Zeta";

void crypt(uint8_t *buf, const long size) {
    for(long i = 0; i < size; i++) {
        uint8_t byte = buf[i];
        for(int j = 0; j < sizeof(KEY); j++) {
            byte = byte ^ KEY[j];
        }
        buf[i] = byte;
    }
}

void usage(void) {
    printf("USAGE:\n");
    printf("    ./crypt <input file> <output file>\n");
}

int main(const int argc, const char *argv[]) {
    if(argc != 3) {
        usage();
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];

    FILE *input_file = fopen(input, "rb");
    if(!input_file) {
        perror("input file open failed");
        return 1;
    }

    fseek(input_file, 0, SEEK_END);
    const long size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    uint8_t *buf = malloc(size);
    if(fread(buf, 1, size, input_file) != size) {
        perror("input file read failed");
        return 1;
    }
    fclose(input_file);

    FILE *output_file = fopen(output, "wb");
    if(!output_file) {
        perror("output file open failed");
        return 1;
    }

    crypt(buf, size);

    if(fwrite(buf, 1, size, output_file) != size) {
        perror("input file read failed");
        return 1;
    }

    free(buf);
    fclose(output_file);

    return 0;
}