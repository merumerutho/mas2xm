// SPDX-License-Identifier: MIT
// mas2xm - MAS binary reader
//
// Parses a .mas binary (song type) into an in-memory Module struct.
// Based on the MAS format spec (mas_spec.md) and mmutil/maxmod source.

#include "mas_read.h"
#include "io_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- helpers ----

void module_free(Module *mod)
{
    if (mod->instruments) {
        free(mod->instruments);
        mod->instruments = NULL;
    }
    if (mod->samples) {
        for (int i = 0; i < mod->samp_count; i++)
            free(mod->samples[i].data);
        free(mod->samples);
        mod->samples = NULL;
    }
    if (mod->patterns) {
        free(mod->patterns);
        mod->patterns = NULL;
    }
}

// ---- envelope reader ----

static int read_envelope(const u8 *buf, size_t size, size_t *pos, Envelope *env)
{
    env->exists = true;

    u8 env_size    = buf_read_u8(buf, size, pos);
    env->loop_start = buf_read_u8(buf, size, pos);
    env->loop_end   = buf_read_u8(buf, size, pos);
    env->sus_start  = buf_read_u8(buf, size, pos);
    env->sus_end    = buf_read_u8(buf, size, pos);
    env->node_count = buf_read_u8(buf, size, pos);
    env->is_filter  = buf_read_u8(buf, size, pos) != 0;
    buf_read_u8(buf, size, pos); // padding (0xBA)

    (void)env_size;

    if (env->node_count > MAX_ENVELOPE_NODES)
        env->node_count = MAX_ENVELOPE_NODES;

    // Reconstruct absolute node positions from delta/base/range encoding
    if (env->node_count > 1) {
        u16 tick = 0;
        for (int i = 0; i < env->node_count; i++) {
            s16 delta  = buf_read_s16(buf, size, pos);
            u16 packed = buf_read_u16(buf, size, pos);

            int base  = packed & 0x7F;
            int range = (packed >> 7) & 0x1FF;

            env->node_x[i] = tick;
            env->node_y[i] = (u8)base;

            tick += (u16)range;

            (void)delta; // not needed for reconstruction (we have base+range)
        }
    } else if (env->node_count == 1) {
        // Single node: no data written by mmutil if node_count <= 1
        // Actually, mmutil only writes nodes if node_count > 1.
        // A single-node envelope has no interpolation data.
        env->node_x[0] = 0;
        env->node_y[0] = 0; // default; will be overridden if data exists
    }

    return 0;
}

// ---- instrument reader ----

static int read_instrument(const u8 *buf, size_t size, size_t base_offset,
                           u32 instr_offset, Instrument *inst)
{
    size_t pos = base_offset + instr_offset;
    size_t inst_start = pos;

    inst->global_volume  = buf_read_u8(buf, size, &pos);
    inst->fadeout        = buf_read_u8(buf, size, &pos);
    inst->random_volume  = buf_read_u8(buf, size, &pos);
    inst->dct            = buf_read_u8(buf, size, &pos);
    inst->nna            = buf_read_u8(buf, size, &pos);
    inst->env_flags      = buf_read_u8(buf, size, &pos);
    inst->panning        = buf_read_u8(buf, size, &pos);
    inst->dca            = buf_read_u8(buf, size, &pos);

    u16 notemap_field    = buf_read_u16(buf, size, &pos);
    buf_read_u16(buf, size, &pos); // reserved

    // Parse envelopes
    memset(&inst->env_vol, 0, sizeof(Envelope));
    memset(&inst->env_pan, 0, sizeof(Envelope));
    memset(&inst->env_pitch, 0, sizeof(Envelope));

    if (inst->env_flags & MAS_ENV_VOL_EXISTS) {
        inst->env_vol.enabled = (inst->env_flags & MAS_ENV_VOL_ENABLED) != 0;
        read_envelope(buf, size, &pos, &inst->env_vol);
    }
    if (inst->env_flags & MAS_ENV_PAN_EXISTS) {
        inst->env_pan.enabled = true; // IT: pan env is enabled if it exists
        read_envelope(buf, size, &pos, &inst->env_pan);
    }
    if (inst->env_flags & MAS_ENV_PITCH_EXISTS) {
        inst->env_pitch.enabled = true;
        read_envelope(buf, size, &pos, &inst->env_pitch);
    }

    // Parse note map
    if (notemap_field & 0x8000) {
        // Single-sample shorthand
        inst->has_notemap = false;
        u16 sample_index = notemap_field & 0x7FFF;
        for (int i = 0; i < MAX_NOTEMAP; i++)
            inst->notemap[i] = (u16)((sample_index << 8) | i);
    } else {
        // Full note map present
        inst->has_notemap = true;
        size_t map_pos = inst_start + notemap_field;
        for (int i = 0; i < MAX_NOTEMAP; i++)
            inst->notemap[i] = buf_read_u16(buf, size, &map_pos);
    }

    return 0;
}

// ---- sample reader ----

static int read_sample(const u8 *buf, size_t size, size_t base_offset,
                       u32 samp_offset, Sample *samp)
{
    size_t pos = base_offset + samp_offset;

    samp->default_volume = buf_read_u8(buf, size, &pos);
    samp->panning        = buf_read_u8(buf, size, &pos);
    samp->frequency      = buf_read_u16(buf, size, &pos);
    samp->vib_type       = buf_read_u8(buf, size, &pos);
    samp->vib_depth      = buf_read_u8(buf, size, &pos);
    samp->vib_speed      = buf_read_u8(buf, size, &pos);
    samp->global_volume  = buf_read_u8(buf, size, &pos);
    samp->vib_rate       = buf_read_u16(buf, size, &pos);
    samp->msl_id         = buf_read_u16(buf, size, &pos);

    samp->data = NULL;
    samp->data_length = 0;

    if (samp->msl_id == 0xFFFF) {
        // Inline NDS sample data
        samp->loop_start   = buf_read_u32(buf, size, &pos);
        samp->loop_length  = buf_read_u32(buf, size, &pos);
        samp->format       = buf_read_u8(buf, size, &pos);
        samp->repeat_mode  = buf_read_u8(buf, size, &pos);
        samp->default_freq = buf_read_u16(buf, size, &pos);
        buf_read_u32(buf, size, &pos); // point (runtime, always 0)

        // Compute data byte count
        u32 sample_count;
        if (samp->repeat_mode == MM_SREPEAT_FORWARD) {
            // looping: total = loop_start + loop_length (in words)
            sample_count = samp->loop_start + samp->loop_length;
        } else {
            // non-looping: length field gives total length in words
            sample_count = samp->loop_length;
        }

        u32 data_bytes;
        if (samp->format == MM_SFORMAT_16BIT) {
            data_bytes = sample_count * 4; // words * 4 bytes, but 16-bit samples -> words = samples/2
        } else {
            data_bytes = sample_count * 4; // words * 4 bytes
        }

        // Add 4 bytes for padding (always present)
        u32 total_bytes = data_bytes + 4;

        if (pos + total_bytes > size) {
            fprintf(stderr, "warning: sample data extends past end of file, truncating\n");
            total_bytes = (u32)(size - pos);
            data_bytes = total_bytes > 4 ? total_bytes - 4 : 0;
        }

        samp->data_length = data_bytes;
        samp->data = malloc(total_bytes);
        if (samp->data) {
            memcpy(samp->data, buf + pos, total_bytes);
        }
    }

    return 0;
}

// ---- pattern reader ----

static int read_pattern(const u8 *buf, size_t size, size_t base_offset,
                        u32 patt_offset, Pattern *patt, bool xm_mode)
{
    size_t pos = base_offset + patt_offset;

    patt->nrows = buf_read_u8(buf, size, &pos) + 1;

    u8 empty_vol = xm_mode ? 0 : 255;

    // Initialize all entries
    for (int i = 0; i < MAX_CHANNELS * 256; i++) {
        patt->data[i].note  = NOTE_EMPTY;
        patt->data[i].inst  = 0;
        patt->data[i].vol   = empty_vol;
        patt->data[i].fx    = 0;
        patt->data[i].param = 0;
    }

    // Per-channel state for mask reuse and value carry-forward
    u8 last_mask[MAX_CHANNELS];
    u8 last_note[MAX_CHANNELS];
    u8 last_inst[MAX_CHANNELS];
    u8 last_vol[MAX_CHANNELS];
    u8 last_fx[MAX_CHANNELS];
    u8 last_param[MAX_CHANNELS];
    memset(last_mask, 0, sizeof(last_mask));
    memset(last_note, NOTE_EMPTY, sizeof(last_note));
    memset(last_inst, 0, sizeof(last_inst));
    memset(last_vol, empty_vol, sizeof(last_vol));
    memset(last_fx, 0, sizeof(last_fx));
    memset(last_param, 0, sizeof(last_param));

    for (int row = 0; row < patt->nrows; row++) {
        while (1) {
            u8 byte = buf_read_u8(buf, size, &pos);
            u8 chan = byte & 0x7F;

            if (chan == 0)
                break; // end of row

            chan -= 1; // 0-based
            if (chan >= MAX_CHANNELS) {
                fprintf(stderr, "warning: invalid channel %d in pattern\n", chan + 1);
                break;
            }

            u8 mask;
            if (byte & 0x80) {
                mask = buf_read_u8(buf, size, &pos);
                last_mask[chan] = mask;
            } else {
                mask = last_mask[chan];
            }

            int idx = row * MAX_CHANNELS + chan;

            // MF flags in upper nibble of mask
            u8 mf = mask >> 4;

            // Read explicitly present fields and update carry-forward state
            if (mask & COMPR_FLAG_NOTE) {
                u8 n = buf_read_u8(buf, size, &pos);
                patt->data[idx].note = n;
                last_note[chan] = n;
            } else if (mf & 0x01) {
                // MF_START set but note not in stream -> reuse last note
                patt->data[idx].note = last_note[chan];
            }

            if (mask & COMPR_FLAG_INSTR) {
                u8 i = buf_read_u8(buf, size, &pos);
                patt->data[idx].inst = i;
                last_inst[chan] = i;
            } else if (mf & 0x02) {
                // MF_DVOL set but instrument not in stream -> reuse last inst
                patt->data[idx].inst = last_inst[chan];
            }

            if (mask & COMPR_FLAG_VOLC) {
                u8 v = buf_read_u8(buf, size, &pos);
                patt->data[idx].vol = v;
                last_vol[chan] = v;
            } else if (mf & 0x04) {
                // MF_HASVCMD set -> reuse last volume command
                patt->data[idx].vol = last_vol[chan];
            }

            if (mask & COMPR_FLAG_EFFC) {
                u8 fx = buf_read_u8(buf, size, &pos);
                u8 pm = buf_read_u8(buf, size, &pos);
                patt->data[idx].fx    = fx;
                patt->data[idx].param = pm;
                last_fx[chan] = fx;
                last_param[chan] = pm;
            } else if (mf & 0x08) {
                // MF_HASFX set -> reuse last effect
                patt->data[idx].fx    = last_fx[chan];
                patt->data[idx].param = last_param[chan];
            }
        }
    }

    return 0;
}

// ---- main reader ----

int mas_read(const u8 *data, size_t size, Module *mod, bool verbose)
{
    memset(mod, 0, sizeof(Module));

    if (size < 8) {
        fprintf(stderr, "error: file too small for MAS prefix\n");
        return 1;
    }

    // ---- Prefix ----
    size_t pos = 0;
    u32 body_size = buf_read_u32(data, size, &pos);
    u8  type      = buf_read_u8(data, size, &pos);
    u8  version   = buf_read_u8(data, size, &pos);
    buf_read_u8(data, size, &pos); // reserved
    buf_read_u8(data, size, &pos); // reserved

    if (type != MAS_TYPE_SONG) {
        fprintf(stderr, "error: not a song MAS file (type=%d)\n", type);
        return 1;
    }

    if (verbose)
        printf("MAS version 0x%02X, body size %u bytes\n", version, body_size);

    // base_offset = start of module header (after prefix)
    size_t base_offset = 8;

    // ---- Module header (276 bytes fixed) ----
    pos = base_offset;
    mod->order_count    = buf_read_u8(data, size, &pos);
    mod->inst_count     = buf_read_u8(data, size, &pos);
    mod->samp_count     = buf_read_u8(data, size, &pos);
    mod->patt_count     = buf_read_u8(data, size, &pos);
    mod->flags          = buf_read_u8(data, size, &pos);
    mod->global_volume  = buf_read_u8(data, size, &pos);
    mod->initial_speed  = buf_read_u8(data, size, &pos);
    mod->initial_tempo  = buf_read_u8(data, size, &pos);
    mod->repeat_position = buf_read_u8(data, size, &pos);
    buf_read_u8(data, size, &pos); // reserved
    buf_read_u8(data, size, &pos); // reserved
    buf_read_u8(data, size, &pos); // reserved

    for (int i = 0; i < MAX_CHANNELS; i++)
        mod->channel_volume[i] = buf_read_u8(data, size, &pos);
    for (int i = 0; i < MAX_CHANNELS; i++)
        mod->channel_panning[i] = buf_read_u8(data, size, &pos);

    for (int i = 0; i < MAX_ORDERS; i++)
        mod->orders[i] = buf_read_u8(data, size, &pos);

    if (verbose) {
        printf("Instruments: %d, Samples: %d, Patterns: %d\n",
               mod->inst_count, mod->samp_count, mod->patt_count);
        printf("Speed: %d, Tempo: %d, Global Vol: %d\n",
               mod->initial_speed, mod->initial_tempo, mod->global_volume);
        printf("Flags: 0x%02X [%s%s%s%s]\n", mod->flags,
               (mod->flags & MAS_FLAG_FREQ_MODE) ? "linear " : "amiga ",
               (mod->flags & MAS_FLAG_XM_MODE) ? "xm " : "",
               (mod->flags & MAS_FLAG_OLD_MODE) ? "old " : "",
               (mod->flags & MAS_FLAG_LINK_GXX) ? "linkgxx" : "");
    }

    // ---- Offset tables ----
    // pos should now be at base_offset + 276 (= 0x114 relative to header)
    u32 *instr_offsets = NULL;
    u32 *samp_offsets  = NULL;
    u32 *patt_offsets  = NULL;

    if (mod->inst_count > 0) {
        instr_offsets = calloc(mod->inst_count, sizeof(u32));
        for (int i = 0; i < mod->inst_count; i++)
            instr_offsets[i] = buf_read_u32(data, size, &pos);
    }
    if (mod->samp_count > 0) {
        samp_offsets = calloc(mod->samp_count, sizeof(u32));
        for (int i = 0; i < mod->samp_count; i++)
            samp_offsets[i] = buf_read_u32(data, size, &pos);
    }
    if (mod->patt_count > 0) {
        patt_offsets = calloc(mod->patt_count, sizeof(u32));
        for (int i = 0; i < mod->patt_count; i++)
            patt_offsets[i] = buf_read_u32(data, size, &pos);
    }

    // ---- Read instruments ----
    if (mod->inst_count > 0) {
        mod->instruments = calloc(mod->inst_count, sizeof(Instrument));
        for (int i = 0; i < mod->inst_count; i++) {
            read_instrument(data, size, base_offset, instr_offsets[i],
                            &mod->instruments[i]);
            if (verbose)
                printf("  Instrument %d: gvol=%d fadeout=%d env_flags=0x%02X\n",
                       i + 1, mod->instruments[i].global_volume,
                       mod->instruments[i].fadeout, mod->instruments[i].env_flags);
        }
    }

    // ---- Read samples ----
    if (mod->samp_count > 0) {
        mod->samples = calloc(mod->samp_count, sizeof(Sample));
        for (int i = 0; i < mod->samp_count; i++) {
            read_sample(data, size, base_offset, samp_offsets[i],
                        &mod->samples[i]);
            if (verbose)
                printf("  Sample %d: vol=%d freq=%d fmt=%d rep=%d len=%u\n",
                       i + 1, mod->samples[i].default_volume,
                       mod->samples[i].frequency * 4,
                       mod->samples[i].format, mod->samples[i].repeat_mode,
                       mod->samples[i].data_length);
        }
    }

    // ---- Read patterns ----
    bool xm_mode = (mod->flags & MAS_FLAG_XM_MODE) != 0;
    if (mod->patt_count > 0) {
        mod->patterns = calloc(mod->patt_count, sizeof(Pattern));
        for (int i = 0; i < mod->patt_count; i++) {
            read_pattern(data, size, base_offset, patt_offsets[i],
                         &mod->patterns[i], xm_mode);
            if (verbose)
                printf("  Pattern %d: %d rows\n", i, mod->patterns[i].nrows);
        }
    }

    free(instr_offsets);
    free(samp_offsets);
    free(patt_offsets);

    return 0;
}
