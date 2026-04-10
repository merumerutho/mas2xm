// SPDX-License-Identifier: MIT
// mas2xm - XM file writer
//
// Writes an in-memory Module to FastTracker II Extended Module (.xm) format.
// This is the inverse of mmutil's Load_XM / CONV_XM_EFFECT.
//
// XM format reference: XM.TXT by Mr.H of Triton / FT2 clone documentation.

#include "xm_write.h"
#include "effects.h"
#include "io_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- helpers ----

// Determine how many channels are actually used across all patterns.
static int count_used_channels(const Module *mod)
{
    int max_chan = 1;
    for (int p = 0; p < mod->patt_count; p++) {
        const Pattern *patt = &mod->patterns[p];
        bool xm_mode = (mod->flags & MAS_FLAG_XM_MODE) != 0;
        u8 empty_vol = xm_mode ? 0 : 255;

        for (int row = 0; row < patt->nrows; row++) {
            for (int ch = MAX_CHANNELS - 1; ch >= max_chan; ch--) {
                const PatternEntry *e = &patt->data[row * MAX_CHANNELS + ch];
                if (e->note != NOTE_EMPTY || e->inst != 0 ||
                    e->vol != empty_vol || e->fx != 0 || e->param != 0) {
                    max_chan = ch + 1;
                    break;
                }
            }
        }
    }
    // XM channel count should be even
    if (max_chan % 2 != 0)
        max_chan++;
    if (max_chan < 2)
        max_chan = 2;
    return max_chan;
}

// Map MAS note to XM note. Returns 0 for empty, 97 for note-off.
static u8 mas_note_to_xm(u8 mas_note)
{
    if (mas_note == NOTE_EMPTY) return 0;
    if (mas_note == NOTE_OFF)   return 97;
    if (mas_note == NOTE_CUT)   return 97; // XM has no note-cut; closest is note-off
    // MAS note 12 = XM note 1 (C-1). MAS note N -> XM note N - 11.
    if (mas_note < 12) return 1; // clamp: below XM range
    int xm = mas_note - 12 + 1;
    if (xm > 96) xm = 96;
    return (u8)xm;
}

// Compute XM relative note and finetune from a frequency in Hz.
static void freq_to_relnote_finetune(u32 freq_hz, s8 *relnote, s8 *finetune)
{
    if (freq_hz == 0) {
        *relnote = 0;
        *finetune = 0;
        return;
    }
    // XM frequency formula: freq = 8363 * 2^(relnote/12 + finetune/1536)
    // Solve: relnote/12 + finetune/1536 = log2(freq / 8363)
    double log2_ratio = log2((double)freq_hz / 8363.0);
    double semitones = log2_ratio * 12.0; // total semitones from middle C

    int rn = (int)round(semitones);
    double ft_frac = (semitones - rn) * 128.0;
    int ft = (int)round(ft_frac);

    // Clamp
    if (rn < -128) rn = -128;
    if (rn > 127)  rn = 127;
    if (ft < -128) ft = -128;
    if (ft > 127)  ft = 127;

    *relnote = (s8)rn;
    *finetune = (s8)ft;
}

// Count how many samples belong to instrument inst_index (0-based).
// In XM, instruments own samples. We need to figure out which samples
// belong to each instrument from the MAS note map.
//
// Returns the list of unique sample indices (0-based, from MAS) and count.
static int get_instrument_samples(const Module *mod, int inst_index,
                                  u8 *sample_list, int max_samples)
{
    const Instrument *inst = &mod->instruments[inst_index];
    int count = 0;

    for (int n = 0; n < MAX_NOTEMAP; n++) {
        u8 samp_idx = (u8)(inst->notemap[n] >> 8);
        if (samp_idx == 0) continue; // no sample

        // Check if already in list
        bool found = false;
        for (int j = 0; j < count; j++) {
            if (sample_list[j] == samp_idx) {
                found = true;
                break;
            }
        }
        if (!found && count < max_samples)
            sample_list[count++] = samp_idx;
    }

    return count;
}

// ---- XM pattern writer ----

static void write_xm_pattern(FILE *f, const Pattern *patt, int nchannels,
                              bool xm_mode)
{
    (void)xm_mode;

    // We write to a temp buffer first to get the packed data length
    // Max possible size: nrows * nchannels * 5 bytes per entry
    size_t max_data = (size_t)patt->nrows * (size_t)nchannels * 5;
    u8 *packed = malloc(max_data);
    size_t packed_len = 0;

    for (int row = 0; row < patt->nrows; row++) {
        for (int ch = 0; ch < nchannels; ch++) {
            const PatternEntry *e = &patt->data[row * MAX_CHANNELS + ch];

            u8 xm_note = mas_note_to_xm(e->note);
            u8 xm_inst = e->inst;
            u8 xm_vol  = e->vol;
            u8 xm_fx = 0, xm_param = 0;

            if (e->fx != 0 || e->param != 0)
                mas_to_xm_effect(e->fx, e->param, &xm_fx, &xm_param);

            // Volume: in XM mode, 0 = empty (already correct).
            // In IT mode, 255 = empty -> convert to 0 for XM
            if (!xm_mode) {
                if (xm_vol == 255)
                    xm_vol = 0;
                else if (xm_vol <= 64)
                    xm_vol = xm_vol + 0x10; // IT 0-64 -> XM set volume 0x10-0x50
            }

            // Determine which fields to pack
            u8 pack_flags = 0x80; // bit 7 = packed format
            if (xm_note != 0) pack_flags |= 0x01;
            if (xm_inst != 0) pack_flags |= 0x02;
            if (xm_vol  != 0) pack_flags |= 0x04;
            if (xm_fx   != 0) pack_flags |= 0x08;
            if (xm_param != 0) pack_flags |= 0x10;

            // If effect type is 0 but param isn't, we must include effect type
            if (xm_param != 0 && xm_fx == 0)
                pack_flags |= 0x08;

            if (pack_flags == 0x80) {
                // Completely empty entry -> just the pack byte
                packed[packed_len++] = pack_flags;
            } else {
                packed[packed_len++] = pack_flags;
                if (pack_flags & 0x01) packed[packed_len++] = xm_note;
                if (pack_flags & 0x02) packed[packed_len++] = xm_inst;
                if (pack_flags & 0x04) packed[packed_len++] = xm_vol;
                if (pack_flags & 0x08) packed[packed_len++] = xm_fx;
                if (pack_flags & 0x10) packed[packed_len++] = xm_param;
            }
        }
    }

    // Write XM pattern header
    fwrite_u32(f, 9);                // header length
    fwrite_u8(f, 0);                 // packing type (always 0)
    fwrite_u16(f, (u16)patt->nrows); // number of rows
    fwrite_u16(f, (u16)packed_len);  // packed data size

    // Write packed data
    fwrite(packed, 1, packed_len, f);

    free(packed);
}

// ---- XM instrument/sample writer ----

// Write sample data in XM delta-encoded format
static void write_xm_sample_data(FILE *f, const Sample *samp)
{
    if (!samp->data || samp->data_length == 0) return;

    bool is_16bit = (samp->format == MM_SFORMAT_16BIT);

    if (is_16bit) {
        u32 num_samples = samp->data_length / 2;
        s16 prev = 0;
        const s16 *src = (const s16 *)samp->data;
        for (u32 i = 0; i < num_samples; i++) {
            s16 cur = src[i];
            s16 delta = (s16)(cur - prev);
            fwrite_s16(f, delta);
            prev = cur;
        }
    } else {
        u32 num_samples = samp->data_length;
        s8 prev = 0;
        const s8 *src = (const s8 *)samp->data;
        for (u32 i = 0; i < num_samples; i++) {
            s8 cur = src[i];
            s8 delta = (s8)(cur - prev);
            fwrite_u8(f, (u8)delta);
            prev = cur;
        }
    }
}

static void write_xm_instrument(FILE *f, const Module *mod, int inst_idx,
                                 bool verbose)
{
    const Instrument *inst = &mod->instruments[inst_idx];

    // Figure out which samples belong to this instrument
    u8 sample_list[256];
    int nsamp = get_instrument_samples(mod, inst_idx, sample_list, 256);

    // Build a local remap: MAS sample index (1-based) -> local XM sample index (0-based)
    u8 samp_remap[256];
    memset(samp_remap, 0, sizeof(samp_remap));
    for (int i = 0; i < nsamp; i++)
        samp_remap[sample_list[i]] = (u8)i;

    if (verbose)
        printf("  Instrument %d: %d samples\n", inst_idx + 1, nsamp);

    // ---- Instrument header ----
    u32 inst_header_size = (nsamp > 0) ? 243 : 29;

    fwrite_u32(f, inst_header_size);

    // Name (22 bytes, empty)
    fwrite_zeros(f, 22);

    fwrite_u8(f, 0); // instrument type (always 0)
    fwrite_u16(f, (u16)nsamp);

    if (nsamp == 0)
        return; // no more data for empty instruments

    // Sample header size
    fwrite_u32(f, 40);

    // Sample map (96 entries)
    // XM notes 0-95 correspond to MAS notes 12-107
    for (int n = 0; n < 96; n++) {
        int mas_note = n + 12;
        u8 mas_samp = (u8)(inst->notemap[mas_note] >> 8);
        fwrite_u8(f, samp_remap[mas_samp]);
    }

    // Volume envelope points (12 points, 4 bytes each = 48 bytes)
    for (int i = 0; i < 12; i++) {
        if (i < inst->env_vol.node_count) {
            fwrite_u16(f, inst->env_vol.node_x[i]);
            fwrite_u16(f, (u16)inst->env_vol.node_y[i]);
        } else {
            fwrite_u16(f, 0);
            fwrite_u16(f, 0);
        }
    }

    // Panning envelope points (12 points)
    for (int i = 0; i < 12; i++) {
        if (i < inst->env_pan.node_count) {
            fwrite_u16(f, inst->env_pan.node_x[i]);
            fwrite_u16(f, (u16)inst->env_pan.node_y[i]);
        } else {
            fwrite_u16(f, 0);
            fwrite_u16(f, 0);
        }
    }

    // Number of volume envelope points
    fwrite_u8(f, inst->env_vol.exists ? inst->env_vol.node_count : 0);
    // Number of panning envelope points
    fwrite_u8(f, inst->env_pan.exists ? inst->env_pan.node_count : 0);

    // Volume sustain/loop points (only meaningful if envelope exists)
    if (inst->env_vol.exists) {
        fwrite_u8(f, inst->env_vol.sus_start);
        fwrite_u8(f, inst->env_vol.loop_start);
        fwrite_u8(f, inst->env_vol.loop_end);
    } else {
        fwrite_u8(f, 0);
        fwrite_u8(f, 0);
        fwrite_u8(f, 0);
    }
    // Panning sustain/loop points
    if (inst->env_pan.exists) {
        fwrite_u8(f, inst->env_pan.sus_start);
        fwrite_u8(f, inst->env_pan.loop_start);
        fwrite_u8(f, inst->env_pan.loop_end);
    } else {
        fwrite_u8(f, 0);
        fwrite_u8(f, 0);
        fwrite_u8(f, 0);
    }

    // Volume envelope flags
    u8 vol_flags = 0;
    if (inst->env_flags & MAS_ENV_VOL_EXISTS) {
        vol_flags |= 1; // on
        if (inst->env_vol.sus_start != 255)  vol_flags |= 2; // sustain
        if (inst->env_vol.loop_start != 255) vol_flags |= 4; // loop
    }
    fwrite_u8(f, vol_flags);

    // Panning envelope flags
    u8 pan_flags = 0;
    if (inst->env_flags & MAS_ENV_PAN_EXISTS) {
        pan_flags |= 1;
        if (inst->env_pan.sus_start != 255)  pan_flags |= 2;
        if (inst->env_pan.loop_start != 255) pan_flags |= 4;
    }
    fwrite_u8(f, pan_flags);

    // Vibrato type, sweep, depth, rate
    // These come from the first sample's auto-vibrato settings
    u8 vib_type = 0, vib_sweep = 0, vib_depth = 0, vib_rate = 0;
    if (nsamp > 0 && sample_list[0] > 0 && sample_list[0] <= mod->samp_count) {
        const Sample *s = &mod->samples[sample_list[0] - 1];
        vib_type = s->vib_type;
        vib_depth = s->vib_depth;
        vib_rate = s->vib_speed;
        // Reverse: vib_rate_mas = 32768 / (xm_vibsweep + 1)
        // -> xm_vibsweep = 32768 / vib_rate_mas - 1
        if (s->vib_rate > 0)
            vib_sweep = (u8)((32768 / s->vib_rate) - 1);
    }

    fwrite_u8(f, vib_type);
    fwrite_u8(f, vib_sweep);
    fwrite_u8(f, vib_depth);
    fwrite_u8(f, vib_rate);

    // Fadeout: MAS stores fadeout/32, XM stores raw. Reverse: * 32
    fwrite_u16(f, (u16)(inst->fadeout * 32));

    // Reserved (2 bytes)
    fwrite_u16(f, 0);

    // ---- Sample headers (one per sample, 40 bytes each) ----
    for (int si = 0; si < nsamp; si++) {
        int mas_idx = sample_list[si] - 1; // 0-based
        if (mas_idx < 0 || mas_idx >= mod->samp_count) {
            // Invalid sample reference, write empty header
            fwrite_zeros(f, 40);
            continue;
        }

        const Sample *samp = &mod->samples[mas_idx];
        bool is_16bit = (samp->format == MM_SFORMAT_16BIT);

        // Compute sample length and loop values in bytes (XM convention)
        u32 sample_length_bytes;
        u32 loop_start_bytes;
        u32 loop_length_bytes;
        u8 loop_bits = 0;

        if (is_16bit) {
            sample_length_bytes = samp->data_length; // already in bytes
            if (samp->repeat_mode == MM_SREPEAT_FORWARD) {
                loop_start_bytes = samp->loop_start * 4; // words -> bytes
                loop_length_bytes = samp->loop_length * 4;
                loop_bits = 1; // forward loop
            } else {
                loop_start_bytes = 0;
                loop_length_bytes = 0;
            }
            loop_bits |= 0x10; // 16-bit flag
        } else {
            sample_length_bytes = samp->data_length;
            if (samp->repeat_mode == MM_SREPEAT_FORWARD) {
                loop_start_bytes = samp->loop_start * 4; // words -> bytes
                loop_length_bytes = samp->loop_length * 4;
                loop_bits = 1;
            } else {
                loop_start_bytes = 0;
                loop_length_bytes = 0;
            }
        }

        fwrite_u32(f, sample_length_bytes);
        fwrite_u32(f, loop_start_bytes);
        fwrite_u32(f, loop_length_bytes);
        fwrite_u8(f, samp->default_volume);

        // Finetune and relative note
        s8 relnote, finetune;
        freq_to_relnote_finetune(samp->frequency * 4, &relnote, &finetune);
        fwrite_u8(f, (u8)finetune);

        fwrite_u8(f, loop_bits); // type (loop + 16bit flags)

        // Panning: MAS stores (xm_pan >> 1) | 128. Reverse:
        u8 xm_pan = (u8)(((samp->panning & 0x7F) << 1));
        fwrite_u8(f, xm_pan);

        fwrite_u8(f, (u8)relnote); // relative note number
        fwrite_u8(f, 0);           // reserved

        // Sample name (22 bytes, empty)
        fwrite_zeros(f, 22);
    }

    // ---- Sample data (delta-encoded) ----
    for (int si = 0; si < nsamp; si++) {
        int mas_idx = sample_list[si] - 1;
        if (mas_idx < 0 || mas_idx >= mod->samp_count)
            continue;

        write_xm_sample_data(f, &mod->samples[mas_idx]);
    }
}

// ---- main writer ----

int xm_write(const Module *mod, const char *filename, bool verbose)
{
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s for writing\n", filename);
        return 1;
    }

    int nchannels = count_used_channels(mod);

    if (verbose)
        printf("Writing XM: %d channels, %d patterns, %d instruments\n",
               nchannels, mod->patt_count, mod->inst_count);

    // ---- XM header (60 + header_size bytes) ----

    // ID text (17 bytes)
    fwrite("Extended Module: ", 1, 17, f);

    // Module name (20 bytes)
    fwrite_zeros(f, 20);

    // 0x1A
    fwrite_u8(f, 0x1A);

    // Tracker name (20 bytes)
    const char *tracker = "mas2xm              ";
    fwrite(tracker, 1, 20, f);

    // Version (1.04)
    fwrite_u16(f, 0x0104);

    // Header size: counts bytes from this field onward to end of header.
    // = 20 bytes of fields + 256 bytes order table = 276
    u32 header_size = 20 + 256;
    fwrite_u32(f, header_size);

    // Determine real order count (up to first 0xFF or max 200)
    int real_order_count = 0;
    for (int i = 0; i < MAX_ORDERS; i++) {
        if (mod->orders[i] == 255) break;
        real_order_count = i + 1;
    }
    if (real_order_count == 0)
        real_order_count = 1;

    fwrite_u16(f, (u16)real_order_count);  // song length
    fwrite_u16(f, (u16)mod->repeat_position); // restart position
    fwrite_u16(f, (u16)nchannels);
    fwrite_u16(f, (u16)mod->patt_count);
    fwrite_u16(f, (u16)mod->inst_count);

    // Flags: bit 0 = linear frequency table
    u16 xm_flags = (mod->flags & MAS_FLAG_FREQ_MODE) ? 1 : 0;
    fwrite_u16(f, xm_flags);

    fwrite_u16(f, (u16)mod->initial_speed);
    fwrite_u16(f, (u16)mod->initial_tempo);

    // Pattern order table (256 bytes in XM, we write 200 then pad)
    for (int i = 0; i < MAX_ORDERS; i++) {
        u8 ord = mod->orders[i];
        if (ord == 255) ord = 0; // XM doesn't have 255 sentinel in order table
        if (ord == 254) ord = 0; // skip markers become 0
        fwrite_u8(f, ord);
    }
    // Pad to 256 entries
    for (int i = MAX_ORDERS; i < 256; i++)
        fwrite_u8(f, 0);

    // ---- Patterns ----
    bool xm_mode = (mod->flags & MAS_FLAG_XM_MODE) != 0;
    for (int i = 0; i < mod->patt_count; i++)
        write_xm_pattern(f, &mod->patterns[i], nchannels, xm_mode);

    // ---- Instruments ----
    for (int i = 0; i < mod->inst_count; i++)
        write_xm_instrument(f, mod, i, verbose);

    fclose(f);

    if (verbose) {
        printf("Written: %s\n", filename);
    }

    return 0;
}
