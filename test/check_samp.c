#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);

    size_t base = 8;
    u8 inst_count = d[base+1], samp_count = d[base+2];
    size_t tbl = base + 276;
    u32 *samp_off = (u32*)(d + tbl + inst_count*4);

    for (int i = 0; i < samp_count && i < 5; i++) {
        size_t pos = base + samp_off[i];
        printf("Sample %d at file offset 0x%zx (rel offset 0x%x):\n", i+1, pos, samp_off[i]);
        printf("  default_vol=%u pan=%u freq=%u\n", d[pos], d[pos+1], *(u16*)(d+pos+2));
        printf("  vib_type=%u vib_depth=%u vib_speed=%u gvol=%u\n",
               d[pos+4], d[pos+5], d[pos+6], d[pos+7]);
        printf("  vib_rate=%u msl_id=0x%04x\n", *(u16*)(d+pos+8), *(u16*)(d+pos+10));

        u16 msl_id = *(u16*)(d+pos+10);
        if (msl_id == 0xFFFF) {
            pos += 12;
            u32 loop_start = *(u32*)(d+pos);
            u32 loop_length = *(u32*)(d+pos+4);
            u8 fmt = d[pos+8];
            u8 rep = d[pos+9];
            u16 dfreq = *(u16*)(d+pos+10);
            u32 point = *(u32*)(d+pos+12);
            printf("  NDS: loop_start=%u loop_length=%u fmt=%u rep=%u dfreq=%u point=%u\n",
                   loop_start, loop_length, fmt, rep, dfreq, point);
            printf("  -> Total words=%u, total bytes=%u\n",
                   loop_start + loop_length, (loop_start + loop_length) * 4);

            // Show first 16 bytes of sample data
            pos += 16;
            printf("  data[0..15]: ");
            for (int j = 0; j < 16; j++) printf("%02x ", d[pos+j]);
            printf("\n");
        }
        printf("\n");
    }

    free(d);
    return 0;
}
