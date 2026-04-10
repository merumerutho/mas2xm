#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);

    u32 header_size = *(u32*)(d + 60);
    u16 npatt = *(u16*)(d + 70);
    u16 ninst = *(u16*)(d + 72);

    size_t pos = 60 + header_size;
    for (int p = 0; p < npatt; p++) {
        u32 phdr = *(u32*)(d + pos);
        u16 dsize = *(u16*)(d + pos + 7);
        pos += phdr + dsize;
    }

    for (int i = 0; i < ninst; i++) {
        if (pos + 29 > (size_t)sz) break;
        u32 ihdr = *(u32*)(d + pos);
        u16 nsamp = *(u16*)(d + pos + 27);

        size_t after_hdr = pos + ihdr;
        if (nsamp > 0 && ihdr >= 33) {
            u32 shdr_sz = *(u32*)(d + pos + 29);
            size_t sp = after_hdr;
            u32 total_data = 0;
            for (int s = 0; s < nsamp; s++) {
                if (sp + 40 > (size_t)sz) break;
                u32 slen = *(u32*)(d + sp);
                u32 ls = *(u32*)(d + sp + 4);
                u32 ll = *(u32*)(d + sp + 8);
                u8 vol = d[sp+12];
                u8 type = d[sp+14];
                int8_t rn = (int8_t)d[sp+16];
                printf("Inst %2d Samp %d: len=%6u ls=%6u ll=%6u vol=%3u type=0x%02x rn=%d\n",
                       i+1, s+1, slen, ls, ll, vol, type, rn);
                total_data += slen;
                sp += shdr_sz;
            }
            pos = sp + total_data;
        } else {
            pos = after_hdr;
        }
    }

    free(d); return 0;
}
