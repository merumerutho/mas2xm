#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

    u32 header_size = *(u32*)(d + 60);
    u16 nchannels = *(u16*)(d + 68);
    u16 npatt = *(u16*)(d + 70);
    u16 ninst = *(u16*)(d + 72);

    // Skip to end of patterns
    size_t pos = 60 + header_size;
    for (int p = 0; p < npatt; p++) {
        u32 phdr = *(u32*)(d + pos);
        u16 dsize = *(u16*)(d + pos + 7);
        pos += phdr + dsize;
    }

    printf("Instruments start at file offset 0x%zx\n\n", pos);

    for (int i = 0; i < ninst && i < 5; i++) {
        size_t inst_start = pos;
        u32 inst_size = *(u32*)(d + pos);
        u16 nsamp = *(u16*)(d + pos + 27);
        printf("Instrument %d at 0x%zx: header_size=%u, nsamples=%u\n",
               i+1, pos, inst_size, nsamp);

        if (nsamp > 0) {
            u32 samp_hdr_size = *(u32*)(d + pos + 29);
            printf("  sample_header_size=%u\n", samp_hdr_size);

            // Show sample map (first 12 entries)
            printf("  sample_map[0..11]: ");
            for (int j = 0; j < 12; j++)
                printf("%d ", d[pos + 33 + j]);
            printf("\n");

            // Volume envelope
            printf("  vol_env_points: ");
            size_t ep = pos + 33 + 96;
            for (int j = 0; j < 3; j++) {
                u16 x = *(u16*)(d + ep + j*4);
                u16 y = *(u16*)(d + ep + j*4 + 2);
                printf("(%u,%u) ", x, y);
            }
            printf("\n");

            u8 vol_npoints = d[pos + 33 + 96 + 96];
            u8 pan_npoints = d[pos + 33 + 96 + 97];
            printf("  vol_env_count=%u pan_env_count=%u\n", vol_npoints, pan_npoints);
            u8 vol_flags = d[pos + 33 + 96 + 96 + 8];
            u8 pan_flags = d[pos + 33 + 96 + 96 + 9];
            printf("  vol_flags=0x%02x pan_flags=0x%02x\n", vol_flags, pan_flags);

            // Fadeout
            u16 fadeout = *(u16*)(d + pos + 33 + 96 + 96 + 14);
            printf("  fadeout=%u\n", fadeout);

            // Jump to sample headers
            pos = inst_start + inst_size;
            for (int s = 0; s < nsamp && s < 3; s++) {
                u32 slen = *(u32*)(d + pos);
                u32 lstart = *(u32*)(d + pos + 4);
                u32 llen = *(u32*)(d + pos + 8);
                u8 vol = d[pos + 12];
                int8_t finetune = (int8_t)d[pos + 13];
                u8 type = d[pos + 14];
                u8 pan = d[pos + 15];
                int8_t relnote = (int8_t)d[pos + 16];
                printf("  Sample %d: len=%u loop_start=%u loop_len=%u vol=%u "
                       "finetune=%d type=0x%02x pan=%u relnote=%d\n",
                       s+1, slen, lstart, llen, vol, finetune, type, pan, relnote);
                pos += samp_hdr_size;
            }

            // Skip sample data
            // Need to go back and read all sample lengths
            pos = inst_start + inst_size;
            u32 total_samp_data = 0;
            for (int s = 0; s < nsamp; s++) {
                u32 slen = *(u32*)(d + pos);
                total_samp_data += slen;
                pos += samp_hdr_size;
            }
            printf("  Total sample data: %u bytes\n", total_samp_data);
            pos += total_samp_data;
        } else {
            pos = inst_start + inst_size;
        }
        printf("\n");
    }

    free(d);
    return 0;
}
