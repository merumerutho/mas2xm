// SPDX-License-Identifier: MIT
// mas2xm - byte-level I/O helpers

#include "io_util.h"
#include <stdlib.h>

u8 buf_read_u8(const u8 *buf, size_t size, size_t *pos)
{
    if (*pos >= size) return 0;
    return buf[(*pos)++];
}

u16 buf_read_u16(const u8 *buf, size_t size, size_t *pos)
{
    u8 lo = buf_read_u8(buf, size, pos);
    u8 hi = buf_read_u8(buf, size, pos);
    return (u16)(lo | (hi << 8));
}

u32 buf_read_u32(const u8 *buf, size_t size, size_t *pos)
{
    u16 lo = buf_read_u16(buf, size, pos);
    u16 hi = buf_read_u16(buf, size, pos);
    return (u32)(lo | ((u32)hi << 16));
}

s16 buf_read_s16(const u8 *buf, size_t size, size_t *pos)
{
    return (s16)buf_read_u16(buf, size, pos);
}

void fwrite_u8(FILE *f, u8 v)
{
    fputc(v, f);
}

void fwrite_u16(FILE *f, u16 v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
}

void fwrite_u32(FILE *f, u32 v)
{
    fwrite_u16(f, (u16)(v & 0xFFFF));
    fwrite_u16(f, (u16)(v >> 16));
}

void fwrite_s16(FILE *f, s16 v)
{
    fwrite_u16(f, (u16)v);
}

void fwrite_zeros(FILE *f, size_t count)
{
    for (size_t i = 0; i < count; i++)
        fputc(0, f);
}

void fwrite_pad(FILE *f, u8 val, size_t count)
{
    for (size_t i = 0; i < count; i++)
        fputc(val, f);
}
