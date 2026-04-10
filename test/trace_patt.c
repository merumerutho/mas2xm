#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t s16;

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz);
    fread(d, 1, sz, f);
    fclose(f);

    size_t base = 8;
    u8 inst_count = d[base+1];
    u8 samp_count = d[base+2];
    u8 patt_count = d[base+3];

    size_t tbl = base + 276;
    u32 *patt_off = (u32*)(d + tbl + inst_count*4 + samp_count*4);

    // Parse pattern 0
    size_t pos = base + patt_off[0];
    int nrows = d[pos] + 1;
    pos++;
    printf("Pattern 0: %d rows\n", nrows);

    u8 last_mask[32] = {0};
    int total_entries = 0;

    for (int row = 0; row < nrows && row < 8; row++) {
        printf("Row %2d: ", row);
        while (1) {
            u8 byte = d[pos++];
            u8 chan = byte & 0x7F;
            if (chan == 0) { printf("[end]\n"); break; }
            chan--;

            u8 mask;
            if (byte & 0x80) {
                mask = d[pos++];
                last_mask[chan] = mask;
            } else {
                mask = last_mask[chan];
            }

            printf("ch%d(m%02x", chan+1, mask);

            if (mask & 0x01) { printf(" n%d", d[pos]); pos++; }
            if (mask & 0x02) { printf(" i%d", d[pos]); pos++; }
            if (mask & 0x04) { printf(" v%02x", d[pos]); pos++; }
            if (mask & 0x08) { printf(" fx%d/%d", d[pos], d[pos+1]); pos+=2; }
            printf(") ");
            total_entries++;
        }
    }
    printf("Total entries in first 8 rows: %d\n", total_entries);

    free(d);
    return 0;
}
