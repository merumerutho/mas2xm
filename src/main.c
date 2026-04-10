// SPDX-License-Identifier: MIT
// mas2xm - MAS to XM converter
//
// Converts maxmod .mas module files back to FastTracker II .xm format.

#include "types.h"
#include "mas_read.h"
#include "xm_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *progname)
{
    fprintf(stderr, "mas2xm v0.1.0 - MAS to XM converter\n");
    fprintf(stderr, "Usage: %s [-v] input.mas output.xm\n", progname);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -v    Verbose output\n");
}

static u8 *read_file(const char *filename, size_t *out_size)
{
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0) {
        fprintf(stderr, "error: empty or invalid file %s\n", filename);
        fclose(f);
        return NULL;
    }

    u8 *data = malloc((size_t)fsize);
    if (!data) {
        fprintf(stderr, "error: out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t nread = fread(data, 1, (size_t)fsize, f);
    fclose(f);

    if (nread != (size_t)fsize) {
        fprintf(stderr, "error: short read on %s\n", filename);
        free(data);
        return NULL;
    }

    *out_size = (size_t)fsize;
    return data;
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    const char *input = NULL;
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (!input) {
            input = argv[i];
        } else if (!output) {
            output = argv[i];
        } else {
            fprintf(stderr, "error: too many arguments\n");
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!input || !output) {
        print_usage(argv[0]);
        return 1;
    }

    // Read MAS file
    size_t mas_size;
    u8 *mas_data = read_file(input, &mas_size);
    if (!mas_data)
        return 1;

    // Parse MAS
    Module mod;
    int ret = mas_read(mas_data, mas_size, &mod, verbose);
    free(mas_data);

    if (ret != 0) {
        fprintf(stderr, "error: failed to parse MAS file\n");
        return 1;
    }

    // Write XM
    ret = xm_write(&mod, output, verbose);
    module_free(&mod);

    if (ret != 0) {
        fprintf(stderr, "error: failed to write XM file\n");
        return 1;
    }

    return 0;
}
