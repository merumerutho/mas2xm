#include "src/types.h"
#include "src/mas_read.h"
#include <stdio.h>
#include <stdlib.h>
int main() {
    FILE *f = fopen("test/sweetdre.mas", "rb");
    fseek(f, 0, SEEK_END); size_t sz = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(sz); fread(d, 1, sz, f); fclose(f);
    Module mod; mas_read(d, sz, &mod, false); free(d);
    for (int i = 0; i < mod.samp_count; i++) {
        Sample *s = &mod.samples[i];
        if (s->vib_depth || s->vib_speed || s->vib_rate)
            printf("Samp %d: type=%d depth=%d speed=%d rate=%d\n",
                   i+1, s->vib_type, s->vib_depth, s->vib_speed, s->vib_rate);
    }
    module_free(&mod); return 0;
}
