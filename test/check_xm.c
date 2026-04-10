#include <stdio.h>
#include <stdlib.h>
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

    // XM header: 60 bytes fixed, then header_size bytes
    u32 header_size = *(u32*)(d + 60);
    u16 song_len = *(u16*)(d + 64);
    u16 nchannels = *(u16*)(d + 68);
    u16 npatt = *(u16*)(d + 70);
    u16 ninst = *(u16*)(d + 72);
    printf("Header size: %u\n", header_size);
    printf("Song length: %u, Channels: %u, Patterns: %u, Instruments: %u\n",
           song_len, nchannels, npatt, ninst);

    // Patterns start at offset 60 + header_size
    size_t pos = 60 + header_size;
    printf("\nPatterns start at file offset 0x%zx\n", pos);

    for (int p = 0; p < npatt && p < 3; p++) {
        u32 patt_hdr_len = *(u32*)(d + pos);
        u8 packing = d[pos + 4];
        u16 nrows = *(u16*)(d + pos + 5);
        u16 data_size = *(u16*)(d + pos + 7);
        printf("Pattern %d: hdr=%u packing=%u rows=%u data_size=%u\n",
               p, patt_hdr_len, packing, nrows, data_size);

        // Dump first few entries
        size_t data_start = pos + patt_hdr_len;
        printf("  First 32 bytes: ");
        for (int i = 0; i < 32 && i < data_size; i++)
            printf("%02x ", d[data_start + i]);
        printf("\n");

        // Decode first row
        size_t dp = data_start;
        printf("  Row 0: ");
        for (int ch = 0; ch < nchannels; ch++) {
            u8 b = d[dp++];
            if (b & 0x80) {
                u8 note=0, inst=0, vol=0, fx=0, param=0;
                if (b & 0x01) note = d[dp++];
                if (b & 0x02) inst = d[dp++];
                if (b & 0x04) vol = d[dp++];
                if (b & 0x08) fx = d[dp++];
                if (b & 0x10) param = d[dp++];
                printf("[n%d i%d v%d fx%d/%d] ", note, inst, vol, fx, param);
            } else {
                u8 note=b, inst=d[dp++], vol=d[dp++], fx=d[dp++], param=d[dp++];
                printf("[N%d I%d V%d FX%d/%d] ", note, inst, vol, fx, param);
            }
        }
        printf("\n");

        pos = data_start + data_size;
    }

    free(d);
    return 0;
}
