#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

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

    // Skip patterns
    size_t pos = 60 + header_size;
    for (int p = 0; p < npatt; p++) {
        u32 phdr = *(u32*)(d + pos);
        u16 dsize = *(u16*)(d + pos + 7);
        printf("  patt %d: pos=0x%zx hdr=%u dsize=%u\n", p, pos, phdr, dsize);
        pos += phdr + dsize;
    }

    printf("\nInstruments should start at 0x%zx (file size=%ld)\n", pos, sz);

    // Read first instrument carefully
    for (int i = 0; i < ninst && i < 3; i++) {
        if (pos + 29 > (size_t)sz) {
            printf("Instrument %d: past EOF at 0x%zx!\n", i+1, pos);
            break;
        }
        u32 inst_hdr_size = *(u32*)(d + pos);
        // Name at pos+4 (22 bytes)
        u8 inst_type = d[pos + 26];
        u16 nsamp = *(u16*)(d + pos + 27);
        printf("Instrument %d at 0x%zx: hdr_size=%u type=%u nsamp=%u\n",
               i+1, pos, inst_hdr_size, inst_type, nsamp);

        if (nsamp > 0 && inst_hdr_size >= 33) {
            u32 samp_hdr_sz = *(u32*)(d + pos + 29);
            printf("  samp_hdr_size=%u\n", samp_hdr_sz);
        }

        // According to XM spec, we skip inst_hdr_size bytes from pos
        // Then read nsamp sample headers (each samp_hdr_size bytes)
        // Then read nsamp sample data blocks
        size_t after_inst_hdr = pos + inst_hdr_size;

        if (nsamp > 0 && inst_hdr_size >= 33) {
            u32 samp_hdr_sz = *(u32*)(d + pos + 29);
            size_t shdr_pos = after_inst_hdr;
            u32 total_data = 0;
            for (int s = 0; s < nsamp; s++) {
                if (shdr_pos + 40 > (size_t)sz) { printf("  sample hdr past EOF!\n"); break; }
                u32 slen = *(u32*)(d + shdr_pos);
                u32 lstart = *(u32*)(d + shdr_pos + 4);
                u32 llen = *(u32*)(d + shdr_pos + 8);
                u8 vol = d[shdr_pos + 12];
                int8_t ft = (int8_t)d[shdr_pos + 13];
                u8 type = d[shdr_pos + 14];
                u8 pan = d[shdr_pos + 15];
                int8_t rn = (int8_t)d[shdr_pos + 16];
                printf("  samp %d: len=%u ls=%u ll=%u vol=%u ft=%d type=0x%02x pan=%u rn=%d\n",
                       s+1, slen, lstart, llen, vol, ft, type, pan, rn);
                total_data += slen;
                shdr_pos += samp_hdr_sz;
            }
            pos = shdr_pos + total_data;
        } else {
            pos = after_inst_hdr;
        }
    }

    free(d);
    return 0;
}
