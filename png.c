#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

uint8_t png_sig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
#define PNG_SIG_SIZE sizeof(png_sig)

void read_bytes_or_panic(FILE *input_file, void *buf, size_t buf_size) {
    size_t n = fread(buf, buf_size, 1, input_file);
    if (n != 1) {
        if (ferror(input_file)) {
            fprintf(stderr, "ERROR: could not read %zu bytes: %s\n",
                    buf_size, strerror(errno));
        } else if (feof(input_file)) {
            fprintf(stderr, "ERROR: could not read %zu bytes: reached the end of file\n",
                    buf_size);
        } else {
            assert(0 && "unreachable");
        }
        exit(1);
    }
}

void write_bytes_or_panic(FILE *file, void *buf, size_t buf_cap) {
     size_t n = fwrite(buf, buf_cap, 1, file);
     if (n != 1) {
         if (ferror(file)) {
             fprintf(stderr, "ERROR: could not write %zu bytes to file: %s\n",
                     buf_cap, strerror(errno));
         } else {
             assert (0 && "unreachable");
         }
     }
}

void print_bytes(uint8_t *buf, size_t size) {
    for (size_t i = 0; i < size; ++i)
        printf("%u ", buf[i]);
    putchar('\n');
}

void reverse_bytes(void *bytes, size_t size) {
    uint8_t item, *buf = bytes;
    for (size_t l = 0, r; l < size / 2; l++) {
        item = buf[l];
        buf[l] = buf[r = size - l - 1];
        buf[r] = item;
    }
}

void usage(FILE *file, char *prog) {
    fprintf(file, "Usage: %s <input.png> <output.png>\n", prog);
}

#define CHUNK_BUF_CAP   (32 * 1024)
uint8_t chunk_buf[CHUNK_BUF_CAP];

int main(int argc, char **argv) {
    (void) argc;
    char *prog = *argv++;
    if (*argv == NULL) {
        usage(stderr, prog);
        fprintf(stderr, "No input file provided\n");
        exit(1);
    }
    char *input_filepath = *argv++;

    if (*argv == NULL) {
        usage(stderr, prog);
        fprintf(stderr, "No output file provided\n");
        exit(1);
    }
    char *output_filepath = *argv++;

    FILE *input_file = fopen(input_filepath, "rb");
    if (input_file == NULL) {
        fprintf(stderr, "ERROR: could not open file %s: %s\n",
                input_filepath, strerror(errno));
        exit(1);
    }

    FILE *output_file = fopen(output_filepath, "wb");
    if (output_file == NULL) {
        fprintf(stderr, "ERROR: could not open file %s: %s\n",
                output_filepath, strerror(errno));
        exit(1);
    }

    uint8_t sig[PNG_SIG_SIZE];
    read_bytes_or_panic(input_file, sig, PNG_SIG_SIZE);
    write_bytes_or_panic(output_file, sig, PNG_SIG_SIZE);
    printf("Signature: ");
    print_bytes(sig, PNG_SIG_SIZE);
    if (memcmp(sig, png_sig, PNG_SIG_SIZE) != 0) {
        fprintf(stderr, "ERROR: %s does not appear to be a valid PNG image\n", input_filepath);
        exit(1);
    }

    bool keep_going = true;
    while (keep_going) {
        uint32_t chunk_sz;
        read_bytes_or_panic(input_file, &chunk_sz, sizeof(chunk_sz));
        write_bytes_or_panic(output_file, &chunk_sz, sizeof(chunk_sz));
        reverse_bytes(&chunk_sz, sizeof(chunk_sz));

        uint8_t chunk_type[4];
        read_bytes_or_panic(input_file, chunk_type, sizeof(chunk_type));
        write_bytes_or_panic(output_file, chunk_type, sizeof(chunk_type));

        if (*(uint32_t*) chunk_type == 0x444E4549) {
            keep_going = false;
        }

        size_t n = chunk_sz;
        while (n > 0) {
            size_t m = (n > CHUNK_BUF_CAP) ? CHUNK_BUF_CAP : n;
            read_bytes_or_panic(input_file, chunk_buf, m);
            write_bytes_or_panic(output_file, chunk_buf, m);
            n -= m;
        }

        uint32_t chunk_crc;
        read_bytes_or_panic(input_file, &chunk_crc, sizeof(chunk_crc));
        write_bytes_or_panic(output_file, &chunk_crc, sizeof(chunk_crc));
        
        if (*(uint32_t*) chunk_type == 0x52444849) {
            char injected_data[] = "YEP";
            uint32_t injected_sz = sizeof(injected_data) - 1; // no null byte
            reverse_bytes(&injected_sz, sizeof(injected_sz));
            write_bytes_or_panic(output_file, &injected_sz, sizeof(injected_sz));
            reverse_bytes(&injected_sz, sizeof(injected_sz));

            char *injected_type = "coCK";
            write_bytes_or_panic(output_file, injected_type, 4);
            write_bytes_or_panic(output_file, injected_data, injected_sz);

            uint32_t injected_crc = 0;
            write_bytes_or_panic(output_file, &injected_crc, sizeof(injected_crc));
        }

        printf("Chunk size: %d\n", chunk_sz);
        printf("Chunk type: %.*s (0x%08X)\n", (int) sizeof(chunk_type), chunk_type,
                *(uint32_t*) chunk_type);
        printf("Chunk  CRC: 0x%08X\n\n", chunk_crc);
    }

    fclose(input_file);
    fclose(output_file);

    return 0;
}
