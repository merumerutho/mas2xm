// SPDX-License-Identifier: MIT
// mas2xm - MAS to XM converter

#ifndef MAS2XM_TYPES_H
#define MAS2XM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

#define MAX_CHANNELS    32
#define MAX_ORDERS      200
#define MAX_ENVELOPE_NODES 25
#define MAX_NOTEMAP     120

// MAS prefix types
#define MAS_TYPE_SONG       0
#define MAS_TYPE_SAMPLE_GBA 1
#define MAS_TYPE_SAMPLE_NDS 2

// MAS version
#define MAS_VERSION 0x18

// Header flags
#define MAS_FLAG_LINK_GXX   (1 << 0)
#define MAS_FLAG_OLD_EFFECTS (1 << 1)
#define MAS_FLAG_FREQ_MODE  (1 << 2)
#define MAS_FLAG_XM_MODE    (1 << 3)
#define MAS_FLAG_MSL_DEP    (1 << 4)
#define MAS_FLAG_OLD_MODE   (1 << 5)

// Instrument envelope flags
#define MAS_ENV_VOL_EXISTS   (1 << 0)
#define MAS_ENV_PAN_EXISTS   (1 << 1)
#define MAS_ENV_PITCH_EXISTS (1 << 2)
#define MAS_ENV_VOL_ENABLED  (1 << 3)

// Sample formats
#define MM_SFORMAT_8BIT  0
#define MM_SFORMAT_16BIT 1
#define MM_SFORMAT_ADPCM 2

// Sample repeat modes
#define MM_SREPEAT_FORWARD 1
#define MM_SREPEAT_OFF     2

// Pattern compression flags
#define COMPR_FLAG_NOTE  (1 << 0)
#define COMPR_FLAG_INSTR (1 << 1)
#define COMPR_FLAG_VOLC  (1 << 2)
#define COMPR_FLAG_EFFC  (1 << 3)

// Special note values
#define NOTE_CUT 254
#define NOTE_OFF 255
#define NOTE_EMPTY 250

// ---- In-memory structures ----

typedef struct {
    u16 node_x[MAX_ENVELOPE_NODES];
    u8  node_y[MAX_ENVELOPE_NODES];
    u8  node_count;
    u8  loop_start;
    u8  loop_end;
    u8  sus_start;
    u8  sus_end;
    bool is_filter;
    bool exists;
    bool enabled;
} Envelope;

typedef struct {
    u8   global_volume;
    u8   fadeout;
    u8   random_volume;
    u8   dct;
    u8   nna;
    u8   env_flags;
    u8   panning;
    u8   dca;
    bool has_notemap;
    u16  notemap[MAX_NOTEMAP]; // hi byte = sample (1-based), lo byte = note

    Envelope env_vol;
    Envelope env_pan;
    Envelope env_pitch;
} Instrument;

typedef struct {
    u8  note;
    u8  inst;
    u8  vol;
    u8  fx;
    u8  param;
} PatternEntry;

typedef struct {
    u16          nrows;
    PatternEntry data[MAX_CHANNELS * 256];
} Pattern;

typedef struct {
    u8   default_volume;
    u8   panning;
    u16  frequency;     // Hz / 4
    u8   vib_type;
    u8   vib_depth;
    u8   vib_speed;
    u8   global_volume;
    u16  vib_rate;
    u16  msl_id;

    // NDS sample data (when msl_id == 0xFFFF)
    u32  loop_start;    // in words
    u32  loop_length;   // in words (or total length if no loop)
    u8   format;        // 0=8bit, 1=16bit, 2=adpcm
    u8   repeat_mode;   // 1=forward, 2=off
    u16  default_freq;  // Hz * 1024 / 32768
    u32  data_length;   // raw data byte count (computed)
    u8  *data;          // raw PCM data (signed), allocated
} Sample;

typedef struct {
    u8   order_count;
    u8   inst_count;
    u8   samp_count;
    u8   patt_count;
    u8   flags;
    u8   global_volume;
    u8   initial_speed;
    u8   initial_tempo;
    u8   repeat_position;
    u8   channel_volume[MAX_CHANNELS];
    u8   channel_panning[MAX_CHANNELS];
    u8   orders[MAX_ORDERS];

    Instrument *instruments;
    Sample     *samples;
    Pattern    *patterns;
} Module;

void module_free(Module *mod);

#endif // MAS2XM_TYPES_H
