#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
int main() {
    FILE *f = fopen("/c/Projects/MAXMXDS/songs/XM/0-insnej.xm", "rb");
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *d = malloc(sz); fread(d, 1, sz, f); fclose(f);
    uint16_t sl = *(uint16_t*)(d+64);
    printf("Orders (%d): ", sl);
    for (int i = 0; i < sl; i++) printf("%d ", d[80+i]);
    printf("\n");
    free(d);
}
