# mas2xm: MAS to XM Converter

> **Inverse of `mmutil`.** Reads a maxmod `.mas` module file and reconstructs a
> FastTracker II `.xm` file from it.
>
> Reference format spec: [`C:\Projects\mas_spec\`](../mas_spec/mas_spec.md).

---

## Table of Contents

1. [What it does (and what it does not)](#1-what-it-does-and-what-it-does-not)
2. [Building](#2-building)
3. [Command line](#3-command-line)
4. [Conversion pipeline](#4-conversion-pipeline)
5. [Source layout](#5-source-layout)
6. [In-memory data model](#6-in-memory-data-model)
7. [MAS reader (`mas_read.c`)](#7-mas-reader-mas_readc)
8. [XM writer (`xm_write.c`)](#8-xm-writer-xm_writec)
9. [Effect reverse mapping (`effects.c`)](#9-effect-reverse-mapping-effectsc)
10. [Known conversion losses](#10-known-conversion-losses)
11. [Validation and tests](#11-validation-and-tests)
12. [Implementation errata](#12-implementation-errata)
13. [Glossary and references](#13-glossary-and-references)

---

## 1. What it does (and what it does not)

`mas2xm` consumes a song-type `.mas` file (MAS prefix `type == 0`) produced by
**mmutil** (the maxmod utility) and emits an `.xm` file that reproduces the
musical content of the original module. It is the deliberate inverse of
mmutil's XM → MAS path: every transformation that mmutil applies in
`Load_XM` / `CONV_XM_EFFECT` / `Write_Sample` is reversed here.

| Reads | Writes |
|---|---|
| `.mas` song module (NDS sample format, `MAS_VERSION = 0x18`) | `.xm` (FastTracker II Extended Module, version `0x0104`) |

**Out of scope (intentionally):**

- Standalone-sample `.mas` files (`type == 1` or `2`).
- GBA-format MAS samples (8-bit unsigned PCM).
- MSL soundbank containers (`*maxmod*`). The reader rejects modules whose
  samples have `msl_id != 0xFFFF` because the data is not embedded.
- Non-XM source formats. mmutil also accepts IT/S3M/MOD; while their MAS
  output is structurally identical, the volume column and effect command
  spaces are interpreted in IT mode rather than XM mode and the output `.xm`
  cannot losslessly carry IT-specific commands. mas2xm decodes both modes
  (it inspects `MAS_FLAG_XM_MODE`) but only XM-mode inputs are validated by
  the test suite.

The format dependency is **maxmod's NDS sample layout**. A `.mas` file built
for GBA cannot be processed because the sample header layout differs
(8 bytes instead of 12+4) and unsigned PCM is not handled.

---

## 2. Building

`mas2xm` is a small C99 project built with CMake.

```bash
cmake -S . -B build
cmake --build build
```

The result is `build/mas2xm` (or `build/mas2xm.exe` on Windows). The build
has no external dependencies beyond the C standard library and `<math.h>`
for `log2` and `round`, which are used in frequency reconstruction.

Tested compilers: GCC (MSYS2/MinGW-w64) and MSVC. Warnings are enabled
(`-Wall -Wextra -pedantic` or `/W4`).

---

## 3. Command line

```
mas2xm v0.1.0 - MAS to XM converter
Usage: mas2xm [-v] input.mas output.xm

Options:
  -v    Verbose output
```

**Example:**

```bash
# Convert a .mas back to .xm
./build/mas2xm test/sweetdre.mas /tmp/sweetdre_recon.xm

# With verbose progress (instrument/sample/pattern dump)
./build/mas2xm -v test/kuk.mas /tmp/kuk_recon.xm
```

Exit codes: `0` on success; `1` if the input cannot be opened, the MAS
prefix is invalid, the type is not a song, the parser fails, or the
output file cannot be written.

---

## 4. Conversion pipeline

```
                ┌──────────────────┐
                │   .mas file      │
                │   (binary)       │
                └────────┬─────────┘
                         │  read_file()  →  raw byte buffer
                         ▼
                ┌──────────────────┐
                │  mas_read()      │  src/mas_read.c
                │                  │
                │  • prefix        │
                │  • header (276)  │
                │  • offset tables │
                │  • instruments   │
                │  • samples (NDS) │
                │  • patterns      │
                └────────┬─────────┘
                         │  in-memory `Module` (src/types.h)
                         ▼
                ┌──────────────────┐
                │  xm_write()      │  src/xm_write.c
                │                  │
                │  • XM header     │
                │  • patterns      │   ← mas_to_xm_effect()
                │  • instruments   │
                │  • samples       │   ← delta-encode + freq_to_relnote_finetune()
                └────────┬─────────┘
                         │  fwrite_*()
                         ▼
                ┌──────────────────┐
                │   .xm file       │
                └──────────────────┘
```

The whole `.mas` is loaded into memory in one shot before parsing, then
serialized straight to disk. There is no streaming and no large
intermediate buffer apart from the `Module` struct itself.

---

## 5. Source layout

```
mas2xm/
├── CMakeLists.txt
├── DOCUMENTATION.md          ← this document
├── TEST_TOOLS.md             ← reference for everything in test/
├── src/
│   ├── main.c                CLI front-end (arg parsing, file I/O)
│   ├── types.h               Module / Pattern / Sample / Instrument structs + constants
│   ├── io_util.c/.h          Little-endian buffer-read and FILE-write helpers
│   ├── mas_read.c/.h         .mas binary  → in-memory `Module`
│   ├── effects.c/.h          MAS effect (IT-letter) → XM numeric effect mapper
│   ├── xm_write.c/.h         in-memory `Module`  → .xm binary
└── test/
    ├── full_cmp.c            structural XM-vs-XM comparator (the reference one)
    ├── batch_test.sh         orchestrates mmutil + mas2xm + full_cmp over a corpus
    ├── roundtrip_test.c      one-for-all unit test (XM → MAS → XM, classifies diffs)
    ├── debug_dump.c          dumps MAS pattern offsets and pattern-0 raw bytes
    ├── trace_patt.c          decodes MAS pattern 0 bypassing mas2xm
    ├── check_xm.c            inspects an .xm file's pattern headers
    ├── check_inst.c          walks XM instrument/sample chain (verbose)
    ├── check_inst2.c         simpler XM inst/samp walker
    ├── check_samp.c          dumps MAS NDS sample headers
    ├── check_all_samp.c      flat list of all sample headers in an .xm
    ├── check_vib.c           reports auto-vibrato fields for sweetdre.mas (links mas2xm lib)
    ├── cmp_orders.c          dumps song length / order list for original vs reconstructed
    ├── quick_orders.c        dumps order list of one hard-coded XM
    ├── cmp_patt.c            dumps decoded pattern-0 entries for original vs reconstructed
    ├── count_hdr.c           prints the canonical XM instrument-header size
    ├── trace_write.c         dumps mas2xm-loaded sample fields for kuk.mas
    ├── kuk.mas, sweetdre.mas pinned reference inputs
    └── kuk_out.xm, sweetdre_out.xm  pinned reference reconstructions
```

---

## 6. In-memory data model

All conversion state lives in a single `Module` struct (`src/types.h`).
Quick map of the most important fields, with the corresponding section of
[`mas_spec.md`](../mas_spec/mas_spec.md):

| Struct | Field | MAS source |
|---|---|---|
| `Module` | `order_count`, `inst_count`, `samp_count`, `patt_count`, `flags`, `global_volume`, `initial_speed`, `initial_tempo`, `repeat_position` | Module Header §4.1 |
| `Module` | `channel_volume[32]`, `channel_panning[32]` | Module Header §4.3 |
| `Module` | `orders[200]` | Pattern Order Table §4.4 |
| `Instrument` | `global_volume`, `fadeout`, `dct`, `nna`, `env_flags`, `panning`, `dca` | Instrument fixed fields §1.1 |
| `Instrument` | `notemap[120]` (high byte = 1-based sample index, low byte = mapped note) | Note Map §4 |
| `Instrument` | `env_vol`, `env_pan`, `env_pitch` | Envelopes §2.2 |
| `Envelope` | `node_x[25]`, `node_y[25]`, `node_count`, `loop_start`, `loop_end`, `sus_start`, `sus_end`, `exists`, `enabled` | Envelope nodes §3 (decoded from delta/base/range) |
| `Sample` | `default_volume`, `panning`, `frequency` (Hz/4), `vib_*`, `global_volume`, `vib_rate`, `msl_id` | Sample Info §1.1 |
| `Sample` | `loop_start`, `loop_length` (in WORDS), `format`, `repeat_mode`, `default_freq`, `data_length`, `data` | NDS Sample Header §2.1 + payload §2.2 |
| `Pattern` | `nrows`, `data[MAX_CHANNELS * 256]` (`PatternEntry { note, inst, vol, fx, param }`) | Patterns §1, §2 |

`PatternEntry.note` uses MAS note values (`0..119`, `254` cut, `255` off,
`250 = NOTE_EMPTY`). `vol` uses the empty sentinel `0` in XM mode and `255`
in IT mode, matching the on-disk convention. Effects are stored in
**MAS letter form** (1=A..26=Z, plus 27..30 extensions); they are translated
to XM numeric form only inside the writer.

`module_free()` releases the dynamic arrays (`instruments`, `samples`,
`patterns`) and each sample's PCM buffer.

---

## 7. MAS reader (`mas_read.c`)

Responsible for taking a `.mas` byte buffer and populating a `Module`. The
reader follows [`mas_spec.md` §2-§5](../mas_spec/mas_spec.md#2-file-layout)
exactly:

### 7.1 Prefix (`§3`)

The first 8 bytes encode `body_size`, `type`, `version`, and two reserved
bytes. The reader rejects anything with `type != MAS_TYPE_SONG (0)` and
records `version` for verbose output (it does not gate on the value, but
the structures assume `0x18`).

The "module base offset" is **`base_offset = 8`**. Every offset stored
inside the file is relative to this point, so the reader uses `base_offset
+ instr_offset[i]` as a file-absolute index when chasing offset tables.

### 7.2 Module header (`§4`)

276 bytes starting at `base_offset`. Layout (in code order):

```
+0x00  order_count, inst_count, samp_count, patt_count
+0x04  flags, global_volume, initial_speed, initial_tempo
+0x08  repeat_position, 3 bytes reserved (0xBA)
+0x0C  channel_volume[32]
+0x2C  channel_panning[32]
+0x4C  orders[200]
+0x114 (offset tables follow)
```

Header flags (`MAS_FLAG_*`): `LINK_GXX`, `OLD_EFFECTS`, `FREQ_MODE`
(linear vs amiga), `XM_MODE`, `MSL_DEP`, `OLD_MODE`. `XM_MODE` selects the
volume column convention; `FREQ_MODE` becomes XM bit-0 of the flags word.

### 7.3 Offset tables (`§5`)

Three contiguous `u32[]` arrays follow the header at relative offset
`0x114`: `instr_offsets`, `samp_offsets`, `patt_offsets`. Sizes come from
`inst_count`, `samp_count`, `patt_count`.

### 7.4 Instruments (`instruments.md`)

Each instrument is `4-byte` aligned. The reader pulls the 12-byte fixed
header, then the 16-bit `notemap_field`:

- **High bit set (`& 0x8000`)** ⇒ single-sample shorthand. Bits 14..0 are
  the 0-based sample index. The reader synthesizes a 120-entry identity
  map: `notemap[i] = (sample << 8) | i`.
- **High bit clear** ⇒ explicit note map. Bits 14..0 are a byte offset
  measured from the start of the instrument struct. The reader seeks to
  `inst_start + notemap_field` and reads 120 `u16` entries.

Envelopes are pulled in order: volume, pan, pitch. Each one is gated by
its matching `*_EXISTS` flag in `env_flags`. `read_envelope()` rebuilds
absolute node X/Y from the delta/base/range encoding by accumulating
`range` into `tick` and stripping `base` from `packed & 0x7F`. The
envelope's `delta` field is intentionally discarded. The integer
`(base, range)` pair is enough to recover the node positions.

### 7.5 Samples (`samples.md`)

Each sample begins with a 12-byte sample-info struct. If `msl_id == 0xFFFF`
the NDS sample header (16 bytes) and PCM payload follow inline; otherwise
the data lives in an MSL soundbank and is unsupported here.

The total payload byte count is computed from the loop fields, treating
them as **WORDS** (per the NDS hardware convention):

```
if repeat_mode == MM_SREPEAT_FORWARD:
    sample_count_words = loop_start + loop_length
else:
    sample_count_words = loop_length    // one-shot length is in loop_length

data_bytes = sample_count_words * 4
```

Both 8-bit and 16-bit formats use the same `* 4` factor because for 16-bit
samples a "word" is 2 samples (each 2 bytes); the math comes out the same.
The reader copies `data_bytes + 4` bytes into `samp->data`, including the
4-byte hardware-interpolation pad that always follows the payload.

### 7.6 Patterns (`patterns.md`)

The single most important piece of the reader and the part most easily
broken: **the IT-style RLE pattern decoder must honor MF carry-forward.**

Per row:

```
loop:
    byte = read_u8
    chan = byte & 0x7F
    if chan == 0: end of row
    if byte & 0x80: mask = read_u8;  last_mask[chan-1] = mask
    else:           mask = last_mask[chan-1]
    mf = mask >> 4

    for each field in (note, inst, vol, effect+param):
        if mask has its COMPR_FLAG_*:
            read field from stream
            update last_*[chan-1] cache
        else if mf has matching MF_* bit:
            field = last_*[chan-1]   // value was suppressed by cache
        else:
            field = empty / 0
```

If you forget the `else if mf & ...` branches the output looks empty: the
classic Errata D.1 from `mas_spec.md`. mas2xm handles all four fields
correctly in `read_pattern()`.

The empty volume sentinel depends on the mode flag:

```
empty_vol = (flags & MAS_FLAG_XM_MODE) ? 0x00 : 0xFF
```

It is used both as the default for empty entries and as the `last_vol`
seed.

---

## 8. XM writer (`xm_write.c`)

The writer rebuilds the standard XM layout from the `Module`:

```
"Extended Module: " (17 bytes)
20 bytes module name (zeroed; MAS doesn't store it)
0x1A
20 bytes tracker name ("mas2xm              ")
u16 version (0x0104)
u32 header_size (276 = 20 fields + 256 order table)
u16 song_length, u16 restart_position
u16 nchannels, u16 npatterns, u16 ninstruments
u16 flags (bit 0 = linear freq table, copied from MAS_FLAG_FREQ_MODE)
u16 default_speed, u16 default_tempo
u8 orders[256]                          ← MAS orders[200] then 56 bytes of zero pad
patterns...
instruments... (header + sample headers + sample data)
```

### 8.1 Channel count

XM does not have a fixed 32 channels; it stores the actual count. mas2xm
walks the patterns once and finds the highest channel that ever has a
non-empty entry (`count_used_channels()`), then **rounds up to an even
number** and clamps to a minimum of 2. This matches XM tooling
expectations.

### 8.2 Pattern packing

For every row × channel pair the writer computes a `pack_flags` byte and
emits only the non-zero fields:

| Bit | Field |
|-----|-------|
| `0x80` | always set ("packed format" marker) |
| `0x01` | note follows |
| `0x02` | instrument follows |
| `0x04` | volume follows |
| `0x08` | effect type follows |
| `0x10` | effect parameter follows |

Edge cases:
- A completely empty entry collapses to a single byte (`0x80`).
- If `xm_param != 0` but the effect type happens to be 0, the writer still
  sets bit `0x08` to make sure the parameter has a "carrier" effect byte
  in the stream. (XM cannot encode "param without effect" otherwise.)
- IT-mode volume bytes (`1..64`, with `255 = empty`) are remapped to the
  XM volume column form `0x10..0x50` before packing.

The packed buffer is sized at `nrows * nchannels * 5` (the worst case),
written into a heap allocation, then preceded in the file by a 9-byte
pattern header (`u32 hdr_len = 9`, `u8 packing = 0`, `u16 nrows`,
`u16 packed_len`).

### 8.3 Notes

Notes are translated by `mas_note_to_xm()`:

```
MAS NOTE_EMPTY (250)  → XM 0  (empty)
MAS NOTE_OFF (255)    → XM 97 (key off)
MAS NOTE_CUT (254)    → XM 97 (closest XM has)
MAS 0..11             → XM 1  (clamped; below XM range)
MAS 12..107           → XM N - 11   (i.e. MAS C-1 → XM C-1)
otherwise             → XM 96 (clamped to top)
```

### 8.4 Instruments and samples

XM puts samples *inside* their owning instrument, while MAS keeps a flat
sample table referenced from a per-instrument note map. To bridge this,
`get_instrument_samples()` walks the MAS note map for each instrument and
collects every distinct sample index it references. Those become the XM
"local" sample list (in encounter order). A `samp_remap[256]` array then
translates MAS-1-based indices into XM-0-based local indices when writing
the 96-entry sample-map field.

The 243-byte XM instrument header (or 29 bytes if `nsamp == 0`) is laid
out per `count_hdr.c` and the comment in `xm_write.c`. Field-by-field:

| XM Field | Source |
|---|---|
| `inst_size` (4) | constant 243 / 29 |
| name (22) | zero-filled (MAS doesn't store names) |
| type (1) | 0 |
| nsamp (2) | from `get_instrument_samples()` |
| `samp_hdr_size` (4) | 40 |
| sample map (96) | from `Instrument.notemap[12..107]` via `samp_remap` |
| vol env points (12 × 4) | `env_vol.node_x[i]`, `env_vol.node_y[i]` (zeros for missing) |
| pan env points (12 × 4) | `env_pan.node_x[i]`, `env_pan.node_y[i]` |
| vol/pan node counts (1+1) | `env_*.node_count` if `exists`, else 0 |
| sustain/loop indices (3+3) | from `env_*.{sus_start, loop_start, loop_end}` if `exists`, else zero |
| vol type (1) | bit 0 = on (always when EXISTS), bit 1 = sustain present, bit 2 = loop present |
| pan type (1) | same encoding |
| vib type/sweep/depth/rate (1+1+1+1) | from the **first sample** of the instrument; sweep is reverse-derived from `vib_rate` (`xm_sweep ≈ 32768/vib_rate − 1`) |
| fadeout (2) | `fadeout * 32` (inverse of mmutil's `/32`) |
| reserved (2) | 0 |

> **Why the envelope flags are derived from `env_flags`, not from the
> sustain/loop fields:** Errata D.4. For instruments without an envelope
> in MAS, the sus/loop bytes are zero-initialized rather than `255`, so
> the obvious test (`sus_start != 255`) would falsely set the sustain
> flag. mas2xm only inspects the sus/loop fields when the matching
> `*_EXISTS` bit is set in `env_flags`.

Each XM sample header is 40 bytes:

```
u32 sample_length_bytes
u32 loop_start_bytes
u32 loop_length_bytes
u8  default_volume
s8  finetune              ← from freq_to_relnote_finetune()
u8  loop_bits             ← bit 0 = forward loop, bit 4 = 16-bit
u8  panning               ← (mas_panning & 0x7F) << 1
s8  relative_note         ← from freq_to_relnote_finetune()
u8  reserved (0)
22  sample name (zeroes)
```

Loop start/length are converted from **MAS words** to **XM bytes** by
multiplying by 4 (the same word factor mentioned in §7.5). Sample-data
length is `samp->data_length` (already in bytes).

### 8.5 Frequency reconstruction

XM stores a **relative note + finetune** pair. mmutil flattens this to a
single Hz value via `8363 * 2^(relnote/12 + finetune/1536)`. The reverse
function `freq_to_relnote_finetune()` undoes it by:

```
semitones = log2(freq_hz / 8363) * 12
relnote   = round(semitones)
finetune  = round((semitones - relnote) * 128)
```

with both fields clamped to `[-128, 127]`. The MAS sample-info field
stores `Hz / 4`, so the writer reconstructs Hz first (`samp->frequency *
4`) before calling the helper. Expect ±1 in the relative note and
±a few in finetune as a normal precision loss.

### 8.6 Sample data: delta encoding

XM uses **per-sample delta encoding** (each value is the difference from
the previous one). mmutil reverses this when reading; we re-apply it in
`write_xm_sample_data()`. The encoding is independently 8-bit or 16-bit
based on the sample format flag.

NDS samples are signed and stored byte-for-byte the same way XM expects
them, so no `+128` shift is needed.

---

## 9. Effect reverse mapping (`effects.c`)

`mas_to_xm_effect()` is the inverse of mmutil's `CONV_XM_EFFECT`. It
takes a `(mas_fx, mas_param)` pair and returns the equivalent XM
`(xm_fx, xm_param)`. The full table is in
[`mas_spec.md effects.md §4`](../mas_spec/effects.md#4-xm-to-mas-effect-mapping)
and the implementation table mirrors it. Highlights:

| MAS letter | MAS byte | XM effect |
|---|---|---|
| `J` | 10 | `0xy` Arpeggio |
| `F` | 6 | `1xx` Porta up; `0xF0\|y` → `E1y`; `0xE0\|y` → `X1y` (effect 33) |
| `E` | 5 | `2xx` Porta down + analogous fine/extra-fine forms |
| `G` | 7 | `3xx` Porta to note |
| `H` | 8 | `4xy` Vibrato |
| `L` | 12 | `5xy` Porta + volslide |
| `K` | 11 | `6xy` Vibrato + volslide |
| `R` | 18 | `7xy` Tremolo |
| `X` | 24 | `8xx` Set panning |
| `O` | 15 | `9xx` Sample offset |
| `D` | 4 | `Axy` Volume slide |
| `B` | 2 | `Bxx` Position jump |
| `C` | 3 | `Dxx` Pattern break (hex → BCD) |
| `A` / `T` | 1 / 20 | `Fxx` Set speed/tempo |
| `V` | 22 | `Gxx` Set global volume |
| `W` | 23 | `Hxx` Global vol slide |
| `P` | 16 | `Pxx` Panning slide |
| `Q` | 17 | `Rxx` Retrigger |
| `S` | 19 | `Exy` Extended (subcommand decoded from high nibble of MAS param) |
| ext 27 | | `Cxx` Set volume |
| ext 28 | | `Kxx` Key off |
| ext 29 | | `Lxx` Set envelope position |
| ext 30 | | `Txx` Tremor |

Notes:

- **Pattern break BCD reversal:** mmutil converts XM's BCD parameter to
  hex when writing MAS (`(xx & 0xF) + (xx >> 4) * 10`). mas2xm does the
  inverse: `((mas_param / 10) << 4) | (mas_param % 10)`.
- **Fine / extra-fine porta:** mmutil flattens both `E1y/E2y` and
  `X1y/X2y` into the `F`/`E` letter range with `0xF0|y` and `0xE0|y`
  parameters. mas2xm prefers the explicit `Exy` / `Xxy` XM forms when
  reconstructing.
- **Set panning ambiguity:** XM has both `8xx` and the 4-bit `E8y`. mas2xm
  always reconstructs the 8-bit `8xx` form because `Xxx` (MAS letter `X`
  byte 24) carries the full byte. This is one of the documented
  "equivalent re-encoding" cases. The original module may have used
  `E8y`, but the playback result is identical.
- **Set speed/tempo merge:** MAS has separate letters `A` (speed) and `T`
  (tempo); XM has only `Fxx` and infers from the parameter. mas2xm emits
  `Fxx` in both directions; the parameter already determines which
  meaning XM picks.
- **Effects with no XM equivalent** (e.g. `S5y`-`SAy` sub-commands)
  return `false` and are written as a zero effect.

---

## 10. Known conversion losses

These are the **expected differences** between an original `.xm` and the
file mas2xm reconstructs from its `.mas`. They are not bugs in mas2xm.
They exist because mmutil discarded or transformed the data when
producing the `.mas`. The full list is in
[`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas)
and is reproduced here as a quick reference.

| Category | Field | Why | Tolerance |
|---|---|---|---|
| Strings | song title, tracker name, instrument & sample names | not stored in MAS | always different (skip) |
| Tracker version | `0x0104` from mas2xm | not stored in MAS | always different (skip) |
| Channels | XM channel count | MAS implicitly 32; mas2xm writes ceil-even | trailing channels are empty |
| Sample length | `+1..+4` bytes | mmutil word-aligns sample data | accept first `len_orig` bytes |
| Loop start/length | `+1..+4` bytes | start padding / loop unrolling | accept |
| Sample type byte | `0x02 → 0x01` | BIDI loop unrolled to forward | accept (data is duplicated) |
| Finetune | `±1..3` | `Hz/4` integer division | accept |
| Relative note | `±1` | complementary to finetune rounding | accept |
| Fadeout | `±31` | `value / 32` integer division | accept |
| Auto-vibrato sweep | up to lossy | `32768 / (sweep+1)` truncated to u8 | very lossy |
| Envelope X positions | `±1 tick / node` | `range` clamped to 511, integer rounding | accept |
| Envelope data on disabled envelopes | absent | mmutil strips if EXISTS bit clear | accept (zero-filled) |
| Effects `E0y / E3y / E5y` | zeroed | unsupported by maxmod | irreversible |
| Porta param ≥ `0xE0` | clamped to `0xDF` | avoids fine/extra-fine collision | irreversible |
| Pattern orders > 200 | dropped | MAS limit is 200 | accept |
| Set panning encoding | `E8y → 8xx` | semantic equivalence | acceptable |

In addition, two **pattern-level effects** of MAS compression cause
extra differences that look like bugs but are not:

1. **Unreferenced patterns.** Patterns that exist in the XM but are
   never referenced by the order table. mmutil emits them but lets the
   compression cache leak across pattern boundaries, so a standalone
   decoder cannot recover the first row of an unreferenced pattern.
   Affected files in the 46-XM corpus include `1fineday.xm`,
   `an-path.xm`, `DEADLOCK.XM`, `clockdead.xm`, and others. The
   classifier in `test/roundtrip_test.c` recognizes both the
   "unreferenced in the order table" case and the "cache leak in a
   referenced pattern" case (Bxx-jump targets, complex pattern loops)
   and marks them as expected.
2. **Note-off + instrument suppression.** If a note-off (XM 97) carries
   an instrument number that happens to match the previous row, mmutil
   clears the `MF_DVOL` bit (note-offs don't trigger default volume)
   *and* the value cache eats the instrument byte. The instrument
   number is then unrecoverable. Audible impact is normally zero.

---

## 11. Validation and tests

The `test/` directory contains a layered set of validation utilities.
**Read [`TEST_TOOLS.md`](TEST_TOOLS.md) for the per-tool reference and
the debugging guide**; this section only lists the high-level workflows.

| Workflow | Driver | What it does |
|---|---|---|
| Smoke test on one file | `mas2xm test/sweetdre.mas /tmp/out.xm` | Convert manually, then load `/tmp/out.xm` in any tracker. |
| Full structural diff | `test/full_cmp.exe orig.xm recon.xm` | Per-section report (header, patterns, samples, envelopes, vibrato). |
| Per-XM roundtrip | `test/roundtrip_test.exe orig.xm` | One-for-all unit test: runs mmutil + mas2xm + full_cmp internally and classifies diffs as **expected** or **unexpected**. Returns `0` only if all diffs are expected. |
| Corpus regression | `bash test/batch_test.sh` | Walks `C:/Projects/MAXMXDS/songs/`, runs the same XM→MAS→XM→cmp pipeline, writes `test/batch_results.log`. |

### 11.1 The canonical regression flow

If you only remember one command, remember this one:

```bash
cmake --build build && \
    for f in C:/Projects/MAXMXDS/songs/BestOf/*.xm; do
        ./test/roundtrip_test.exe "$f" >/dev/null 2>&1 || echo "FAIL: $f"
    done
```

That builds mas2xm, then runs the one-for-all roundtrip on every file
in the `BestOf/` corpus. Anything that prints `FAIL:` is a regression.
At the time of writing all 46 files in `MAXMXDS/songs/` pass with
0 unexpected diffs.

### 11.2 What the test classifier knows

The diff classifier in `test/roundtrip_test.c` codifies every "expected
loss" listed in [`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas)
plus four pattern-level categories (unreferenced patterns, note-off
with cached instrument, mmutil-zeroed effects, and equivalent
re-encoding) that surface when running the corpus through mmutil and
back. Anything the classifier doesn't recognize is treated as a real
bug. The full
categorization table and the procedure for extending it live in
[`TEST_TOOLS.md` § Debugging a failing roundtrip](TEST_TOOLS.md#debugging-a-failing-roundtrip).

### 11.3 Building the test

The CMake target `roundtrip_test` is built by default:

```bash
cmake -S . -B build
cmake --build build      # produces both build/mas2xm.exe and build/roundtrip_test.exe
```

If you have a known XM you always want to test, configure with
`-DMAS2XM_TEST_XM=<absolute_path>` and `ctest` will pick it up:

```bash
cmake -S . -B build -DMAS2XM_TEST_XM=C:/Projects/MAXMXDS/songs/BestOf/sweetdre.xm
cmake --build build
ctest --test-dir build --output-on-failure
```

If you'd rather rebuild it ad-hoc the way the other test tools are
built, just run `gcc`:

```bash
gcc -O2 -o test/roundtrip_test.exe test/roundtrip_test.c
```

The test has no dependencies beyond the C standard library and the
host's C runtime (`<sys/stat.h>`, `<process.h>` on Windows or
`<unistd.h>`/`<sys/wait.h>` on POSIX).

---

## 12. Implementation errata

These are bugs that *were* in mas2xm at some point during development
and are now fixed. They live here so future implementers don't fall
into the same traps. The same list lives in
[`mas_spec.md` Appendix D](../mas_spec/mas_spec.md#appendix-d-implementation-errata).

### D.1 Pattern decompression: MF flag carry-forward (Critical)

Reconstructed patterns appear mostly empty unless the reader honors the
`mask >> 4` (MF) bits and reuses the per-channel `last_*` cache when the
matching `COMPR_FLAG_*` bit is clear. See `read_pattern()` in
`src/mas_read.c` and §7.6 above.

### D.2 XM instrument header size

The XM "instrument size" field must be `243` for instruments with
samples and `29` for empty ones. Any other value silently shifts every
subsequent instrument and sample by the wrong amount.
`test/count_hdr.c` exists to print the canonical 243 calculation.

### D.3 XM header size field

The XM header size field at offset 60 counts bytes from offset 60 to the
end of the header. The correct value is `20 + 256 = 276`. The order table
is **always 256 bytes** in XM even though only `song_length` of them
matter. mas2xm writes `MAS_ORDERS[0..199]` then 56 zero bytes.

### D.4 Envelope flags for non-existent envelopes

Sustain/loop flags for an instrument without a volume or pan envelope
must come out as 0, not be derived from `sus_start != 255` (which is
true for the zero-initialized fields of an envelope that doesn't exist
in MAS). mas2xm only consults sus/loop fields when the matching
`*_EXISTS` bit is set. See `xm_write.c` lines ~313-329.

---

## 13. Glossary and references

| Term | Meaning |
|---|---|
| MAS | maxmod's binary module format. Spec at `C:/Projects/mas_spec/`. |
| mmutil | The maxmod utility that converts XM/IT/S3M/MOD into MAS. Source: `C:/Projects/mmutil/`. |
| maxmod | The runtime sound system for NDS/GBA that plays MAS files. |
| MSL | Maxmod Sound Library. A container that bundles multiple MAS files into a soundbank with sample deduplication. |
| XM | FastTracker II Extended Module format. Reference: `XM.TXT` by Mr.H of Triton. |
| `CONV_XM_EFFECT` | mmutil's XM-to-MAS effect mapper in `xm.c`. mas2xm's `mas_to_xm_effect()` is its inverse. |
| MF flag | Pre-computed semantic flag in the upper nibble of a MAS pattern mask byte. Tells the reader that a field is present even if its value byte was suppressed by the cache. |
| BIDI | Bidirectional sample loop. Unrolled to a forward loop by mmutil because maxmod hardware can't play it. |

**External documents:**

- [`mas_spec/mas_spec.md`](../mas_spec/mas_spec.md): top-level binary format and four appendices (data types, source references, known XM → MAS losses, common implementer pitfalls).
- [`mas_spec/instruments.md`](../mas_spec/instruments.md): instrument layout and envelope encoding.
- [`mas_spec/samples.md`](../mas_spec/samples.md): NDS sample header and alignment rules.
- [`mas_spec/patterns.md`](../mas_spec/patterns.md): pattern RLE compression and the canonical decode pseudocode.
- [`mas_spec/effects.md`](../mas_spec/effects.md): IT-letter numbering and the `CONV_XM_EFFECT` table.
- [`mas_spec/msl_format.md`](../mas_spec/msl_format.md): MSL container.

**Licenses:** mas2xm is MIT. mmutil and maxmod are ISC. mas_spec is
CC-BY-4.0. See [`ACKNOWLEDGMENTS.md`](ACKNOWLEDGMENTS.md) for the full
credit chain and the relationship between the four projects.
