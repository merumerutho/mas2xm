#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

void dump_xm(const char *label, const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) { printf("Cannot open %s\n", file); return; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);

    u32 hdr_size = *(u32*)(d + 60);
    u16 song_len = *(u16*)(d + 64);
    u16 restart = *(u16*)(d + 66);
    u16 nch = *(u16*)(d + 68);
    u16 npatt = *(u16*)(d + 70);
    u16 ninst = *(u16*)(d + 72);
    u16 speed = *(u16*)(d + 76);
    u16 tempo = *(u16*)(d + 78);

    printf("%s: len=%u restart=%u ch=%u patt=%u inst=%u spd=%u tempo=%u\n",
           label, song_len, restart, nch, npatt, ninst, speed, tempo);
    printf("  orders[0..20]: ");
    for (int i = 0; i < 20 && i < song_len; i++)
        printf("%d ", d[80 + i]);
    printf("...\n\n");
    free(d);
}

int main() {
    dump_xm("ORIGINAL", "C:/Projects/MAXMXDS/songs/BestOf/sweetdre.xm");
    dump_xm("RECONSTR", "test/sweetdre_out.xm");
    return 0;
}
