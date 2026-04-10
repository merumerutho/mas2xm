#include "src/types.h"
#include "src/mas_read.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("test/kuk.mas", "rb");
    fseek(f, 0, SEEK_END); size_t sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);

    Module mod;
    mas_read(d, sz, &mod, false);
    free(d);

    printf("inst_count=%d samp_count=%d\n", mod.inst_count, mod.samp_count);
    for (int i = 0; i < mod.samp_count && i < 5; i++) {
        Sample *s = &mod.samples[i];
        printf("Sample %d: data_length=%u loop_start=%u loop_length=%u fmt=%d rep=%d data=%p\n",
               i+1, s->data_length, s->loop_start, s->loop_length, s->format, s->repeat_mode,
               (void*)s->data);
    }

    // Check what get_instrument_samples would return for inst 0
    printf("\nInstrument 1 notemap[12..23]: ");
    for (int n = 12; n < 24; n++)
        printf("0x%04x ", mod.instruments[0].notemap[n]);
    printf("\n");

    module_free(&mod);
    return 0;
}
