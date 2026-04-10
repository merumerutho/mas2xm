// SPDX-License-Identifier: MIT
// mas2xm - byte-level I/O helpers

#ifndef MAS2XM_IO_UTIL_H
#define MAS2XM_IO_UTIL_H

#include "types.h"
#include <stdio.h>

// ---- Read helpers (little-endian) ----

// Read from a byte buffer at a given offset, advancing the offset.
u8  buf_read_u8(const u8 *buf, size_t size, size_t *pos);
u16 buf_read_u16(const u8 *buf, size_t size, size_t *pos);
u32 buf_read_u32(const u8 *buf, size_t size, size_t *pos);
s16 buf_read_s16(const u8 *buf, size_t size, size_t *pos);

// ---- Write helpers (little-endian, to FILE) ----

void fwrite_u8(FILE *f, u8 v);
void fwrite_u16(FILE *f, u16 v);
void fwrite_u32(FILE *f, u32 v);
void fwrite_s16(FILE *f, s16 v);
void fwrite_zeros(FILE *f, size_t count);
void fwrite_pad(FILE *f, u8 val, size_t count);

#endif // MAS2XM_IO_UTIL_H
