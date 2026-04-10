#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

typedef struct { u8 note,inst,vol,fx,param; } Entry;

void decode_row(u8 *d, size_t *dp, int nch, Entry *row) {
    for (int ch = 0; ch < nch; ch++) {
        row[ch] = (Entry){0,0,0,0,0};
        u8 b = d[(*dp)++];
        if (b & 0x80) {
            if (b & 0x01) row[ch].note = d[(*dp)++];
            if (b & 0x02) row[ch].inst = d[(*dp)++];
            if (b & 0x04) row[ch].vol  = d[(*dp)++];
            if (b & 0x08) row[ch].fx   = d[(*dp)++];
            if (b & 0x10) row[ch].param= d[(*dp)++];
        } else {
            row[ch].note=b;
            row[ch].inst=d[(*dp)++]; row[ch].vol=d[(*dp)++];
            row[ch].fx=d[(*dp)++]; row[ch].param=d[(*dp)++];
        }
    }
}

void show_patt0(const char *label, const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);

    u32 hdr_size = *(u32*)(d + 60);
    u16 nch = *(u16*)(d + 68);

    // First pattern (pattern 6 = order[0])
    // But let's just decode physical pattern 0
    size_t pos = 60 + hdr_size;
    u32 phdr = *(u32*)(d + pos);
    u16 nrows = *(u16*)(d + pos + 5);
    printf("%s pattern 0 (%d rows, %d ch):\n", label, nrows, nch);

    size_t dp = pos + phdr;
    Entry row[32];
    for (int r = 0; r < 4 && r < nrows; r++) {
        decode_row(d, &dp, nch, row);
        printf("  row %d: ", r);
        for (int c = 0; c < nch; c++) {
            Entry *e = &row[c];
            if (e->note || e->inst || e->vol || e->fx || e->param)
                printf("c%d[%d,%d,%02x,%d/%d] ", c+1, e->note, e->inst, e->vol, e->fx, e->param);
        }
        printf("\n");
    }
    printf("\n");
    free(d);
}

int main() {
    show_patt0("ORIG", "C:/Projects/MAXMXDS/songs/BestOf/sweetdre.xm");
    show_patt0("RECO", "test/sweetdre_out.xm");
    return 0;
}
