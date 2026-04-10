#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz);
    fread(d, 1, sz, f);
    fclose(f);

    // Prefix
    size_t base = 8;
    u8 inst_count = d[base+1];
    u8 samp_count = d[base+2];
    u8 patt_count = d[base+3];
    u8 flags = d[base+4];
    printf("inst=%d samp=%d patt=%d flags=0x%02x\n", inst_count, samp_count, patt_count, flags);

    // Offset tables start at base+276
    size_t tbl = base + 276;
    u32 *inst_off = (u32*)(d + tbl);
    u32 *samp_off = (u32*)(d + tbl + inst_count*4);
    u32 *patt_off = (u32*)(d + tbl + inst_count*4 + samp_count*4);

    printf("\nPattern offsets (relative to base=0x%zx):\n", base);
    for (int i = 0; i < patt_count; i++) {
        u32 off = patt_off[i];
        printf("  Pattern %d: offset=0x%x (file=0x%zx)\n", i, off, base+off);
    }

    // Dump first pattern data
    printf("\nPattern 0 raw bytes:\n");
    size_t p0 = base + patt_off[0];
    printf("  row_count byte = %d (= %d rows)\n", d[p0], d[p0]+1);
    // Dump next 64 bytes of pattern data
    for (int i = 0; i < 128 && p0+1+i < (size_t)sz; i++) {
        if (i % 16 == 0) printf("  %04x: ", i);
        printf("%02x ", d[p0+1+i]);
        if (i % 16 == 15) printf("\n");
    }
    printf("\n");

    free(d);
    return 0;
}
