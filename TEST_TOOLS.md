# mas2xm Test Tools Reference

> Companion to [`DOCUMENTATION.md`](DOCUMENTATION.md). Describes every
> executable in `test/`: what it does, how to build it, how to invoke
> it, and what its output looks like. The tools split roughly into
> three groups:
>
> 1. **Validation drivers**: `roundtrip_test`, `full_cmp`, `batch_test.sh`.
> 2. **MAS inspectors**: `debug_dump`, `trace_patt`, `check_samp`,
>    `check_vib`, `trace_write`.
> 3. **XM inspectors**: `check_xm`, `check_inst`, `check_inst2`,
>    `check_all_samp`, `cmp_orders`, `cmp_patt`, `quick_orders`, `count_hdr`.

---

## Coverage matrix

This matrix is the answer to "what is actually validated?" Read it
top-to-bottom to see what each format component is exercised by, and
left-to-right to see whether a component is tested manually
(inspectors), structurally (`full_cmp`), or with classified
expected/unexpected diff verdicts (`roundtrip_test`).

The verdict column at the right tells you whether each row is
fully covered (`✓`), partially covered (`~`), or out of scope (`✗`).
"Source file" lists the file(s) in `src/` that own the read or write
logic for the component.

### MAS-side reading (mas_read.c)

| MAS component                       | Source file              | Manual inspector              | Structural diff | Classifier (`roundtrip_test`)               | Verdict |
|-------------------------------------|--------------------------|-------------------------------|-----------------|----------------------------------------------|---------|
| Prefix (size/type/version)          | `mas_read.c`             | `debug_dump`                  | n/a             | implicit (test fails on bad prefix)          | ✓ |
| Module header (276-byte block)      | `mas_read.c`             | `debug_dump`, `check_samp`    | `full_cmp` HEADER | header section of `compare_header()`        | ✓ |
| Header flags (XM_MODE / FREQ_MODE)  | `mas_read.c`             | `debug_dump`                  | `full_cmp` HEADER | `compare_header()` `linear-freq flag`       | ✓ |
| Channel volume / panning (32 each)  | `mas_read.c`             | (none)                        | `full_cmp` HEADER | implicit (would surface as channel diff)   | ~ (read but not field-by-field compared) |
| Order table (200 bytes)             | `mas_read.c`             | `cmp_orders`, `quick_orders`  | `full_cmp` HEADER | `compare_header()` orders section            | ✓ |
| Instrument offset table             | `mas_read.c`             | `debug_dump`                  | n/a             | implicit (drives downstream walk)            | ✓ |
| Sample offset table                 | `mas_read.c`             | `debug_dump`                  | n/a             | implicit                                     | ✓ |
| Pattern offset table                | `mas_read.c`             | `debug_dump`                  | n/a             | implicit                                     | ✓ |
| Instrument fixed fields             | `mas_read.c`             | `check_inst`                  | `full_cmp` SAMPLES | inst nsamp / vol / pan checks               | ✓ |
| Instrument note map (full)          | `mas_read.c`             | `trace_write`                 | indirect via samples | `compare_samples()` (orphan-sample case)   | ✓ |
| Instrument note map (shorthand)     | `mas_read.c`             | `trace_write`                 | indirect         | implicit                                     | ✓ |
| Volume envelope                     | `mas_read.c`             | `check_inst`                  | `full_cmp` ENVELOPES | `compare_envelopes_and_vibrato()`           | ✓ |
| Panning envelope                    | `mas_read.c`             | `check_inst`                  | `full_cmp` ENVELOPES | `compare_envelopes_and_vibrato()`           | ✓ |
| Pitch envelope                      | `mas_read.c`             | (none)                        | (none)          | (none, XM has no pitch env)                  | ~ (parsed but not validated) |
| Sample info struct                  | `mas_read.c`             | `check_samp`, `trace_write`   | `full_cmp` SAMPLES | `compare_samples()`                         | ✓ |
| NDS sample header (16 B)            | `mas_read.c`             | `check_samp`                  | `full_cmp` SAMPLES | `compare_samples()` length / loop checks    | ✓ |
| NDS 8-bit PCM payload               | `mas_read.c`             | `check_samp` (first 16 bytes) | partial         | length only, not byte content                | ~ (length checked, content not byte-compared) |
| NDS 16-bit PCM payload              | `mas_read.c`             | `check_samp` (first 16 bytes) | partial         | length only                                  | ~ (length checked, content not byte-compared) |
| NDS ADPCM payload                   | (not implemented)        | (none)                        | (none)          | (none)                                       | ✗ (out of scope) |
| GBA sample data                     | (rejected by reader)     | (none)                        | (none)          | (none)                                       | ✗ (out of scope) |
| MSL soundbank wrapper               | (rejected by reader)     | (none)                        | (none)          | (none)                                       | ✗ (out of scope) |
| Pattern row count                   | `mas_read.c`             | `debug_dump`, `trace_patt`    | `full_cmp` PATTERNS | `compare_patterns()` row count check        | ✓ |
| Pattern RLE channel byte            | `mas_read.c`             | `trace_patt`                  | `full_cmp` PATTERNS | implicit (drives entry comparison)           | ✓ |
| Pattern mask byte (COMPR_FLAG bits) | `mas_read.c`             | `trace_patt`                  | `full_cmp` PATTERNS | implicit                                     | ✓ |
| Pattern mask byte (MF carry-forward)| `mas_read.c`             | (none; `trace_patt` is the negative test) | `full_cmp` PATTERNS | implicit (Errata D.1)          | ✓ |
| Pattern note field                  | `mas_read.c`             | `trace_patt`                  | `full_cmp` PATTERNS | `compare_patterns()` per-entry              | ✓ |
| Pattern instrument field            | `mas_read.c`             | `trace_patt`                  | `full_cmp` PATTERNS | `compare_patterns()` per-entry              | ✓ |
| Pattern volume column (XM mode)     | `mas_read.c`             | `trace_patt`                  | `full_cmp` PATTERNS | `compare_patterns()` per-entry              | ✓ |
| Pattern volume column (IT mode)     | `mas_read.c`             | (none)                        | `full_cmp` PATTERNS | partial (IT inputs untested)                 | ~ |
| Pattern effect + parameter          | `mas_read.c`, `effects.c`| `trace_patt`                  | `full_cmp` PATTERNS | `compare_patterns()` + reverse mapping      | ✓ |

### XM-side writing (xm_write.c)

| XM component                        | Source file              | Manual inspector              | Structural diff | Classifier verdict source                    | Verdict |
|-------------------------------------|--------------------------|-------------------------------|-----------------|----------------------------------------------|---------|
| File header (`Extended Module:`)    | `xm_write.c`             | `check_xm`                    | n/a             | implicit (load_xm fails on bad header)       | ✓ |
| Module name (zeroed)                | `xm_write.c`             | `cmp_orders`                  | `full_cmp` HEADER | `compare_header()` title (always expected)  | ✓ |
| Tracker name                        | `xm_write.c`             | `cmp_orders`                  | `full_cmp` HEADER | always-expected diff                         | ✓ |
| Header size field (276)             | `xm_write.c`             | `check_xm`                    | implicit         | implicit (Errata D.3)                       | ✓ |
| Song length / restart / channels    | `xm_write.c`             | `cmp_orders`                  | `full_cmp` HEADER | `compare_header()`                          | ✓ |
| Patterns / instruments counts       | `xm_write.c`             | `check_xm`, `check_inst`      | `full_cmp` HEADER | `compare_header()`                          | ✓ |
| Linear-frequency flag               | `xm_write.c`             | (none)                        | `full_cmp` HEADER | `compare_header()` flag bit 0               | ✓ |
| Initial speed / tempo               | `xm_write.c`             | `cmp_orders`                  | `full_cmp` HEADER | `compare_header()`                          | ✓ |
| Order table (256 bytes, padded)     | `xm_write.c`             | `cmp_orders`, `quick_orders`  | `full_cmp` HEADER | `compare_header()` orders section            | ✓ |
| Pattern header (9 bytes)            | `xm_write.c`             | `check_xm`                    | implicit         | `compare_patterns()` (load fails on bad)    | ✓ |
| Pattern packed entries              | `xm_write.c`             | `check_xm`, `cmp_patt`        | `full_cmp` PATTERNS | `compare_patterns()` per-entry              | ✓ |
| Volume column re-encoding (IT→XM)   | `xm_write.c`             | (none)                        | `full_cmp` PATTERNS | partial (IT inputs untested)                 | ~ |
| Effect reverse mapping              | `effects.c`              | (none; see Effects coverage below) | `full_cmp` PATTERNS | `compare_patterns()` per-entry               | ✓ |
| Channel count rounding              | `xm_write.c`             | `check_xm`                    | `full_cmp` HEADER | `compare_header()` (rounded/shrunk cases)   | ✓ |
| Instrument header (243 / 29 bytes)  | `xm_write.c`             | `check_inst`, `check_inst2`   | `full_cmp` SAMPLES | implicit (Errata D.2)                       | ✓ |
| Sample-map field (96 bytes)         | `xm_write.c`             | `check_inst`                  | indirect         | implicit (orphan samples → expected diff)    | ✓ |
| Volume envelope points (12 × 4)     | `xm_write.c`             | `check_inst`                  | `full_cmp` ENVELOPES | `compare_envelopes_and_vibrato()` X / Y     | ✓ |
| Panning envelope points (12 × 4)    | `xm_write.c`             | `check_inst`                  | `full_cmp` ENVELOPES | `compare_envelopes_and_vibrato()` count + flags | ~ (X/Y not loop-compared, only vol env is) |
| Envelope flags derivation           | `xm_write.c`             | `check_inst`                  | `full_cmp` ENVELOPES | `compare_envelopes_and_vibrato()` (Errata D.4) | ✓ |
| Vibrato fields                      | `xm_write.c`             | `check_vib`                   | `full_cmp` VIBRATO | `compare_envelopes_and_vibrato()`           | ✓ |
| Fadeout (×32)                       | `xm_write.c`             | `check_inst`                  | `full_cmp` VIBRATO | `compare_envelopes_and_vibrato()` + cap detection | ✓ |
| XM sample header (40 bytes)         | `xm_write.c`             | `check_inst`, `check_all_samp`| `full_cmp` SAMPLES | `compare_samples()`                         | ✓ |
| Loop start/length (words → bytes)   | `xm_write.c`             | `check_all_samp`              | `full_cmp` SAMPLES | `compare_samples()` + drop / unroll branches | ✓ |
| Sample volume / panning             | `xm_write.c`             | `check_inst`                  | `full_cmp` SAMPLES | `compare_samples()` (volume exact, pan ±2)  | ✓ |
| Relnote / finetune (joint)          | `xm_write.c`             | `check_inst`                  | `full_cmp` SAMPLES | `compare_samples()` joint check (cents)     | ✓ |
| Sample data (delta encoded)         | `xm_write.c`             | (none)                        | n/a             | length only, content NOT byte-compared       | ~ (could miss a delta-encoding bug) |

### Effect mapping (effects.c)

| MAS effect | XM target | Test exposure |
|---|---|---|
| `A` Set Speed | `Fxx` (xx<32) | covered by any file using set-speed |
| `B` Position Jump | `Bxx` | covered by all multi-pattern files |
| `C` Pattern Break (hex→BCD) | `Dxx` | covered by all multi-pattern files |
| `D` Volume Slide | `Axy` | covered by typical XM input |
| `E` / `F` Pitch Slide ± (normal) | `2xx` / `1xx` | covered |
| `E` / `F` Pitch Slide ± (fine, ≥0xF0) | `E2y` / `E1y` | covered |
| `E` / `F` Pitch Slide ± (extra fine, ≥0xE0) | `X2y` / `X1y` | covered |
| `G` Porta to Note | `3xx` | covered |
| `H` Vibrato | `4xy` | covered |
| `J` Arpeggio | `0xy` | covered |
| `K` Vibrato + VolSlide | `6xy` | covered |
| `L` Porta + VolSlide | `5xy` | covered |
| `O` Sample Offset | `9xx` | covered |
| `P` Panning Slide | `Pxx` | covered |
| `Q` Retrigger | `Rxx` | covered |
| `R` Tremolo | `7xy` | covered |
| `S00`–`S0F` Fine vol up | `EAy` | covered |
| `S10`–`S1F` Fine vol down | `EBy` | covered |
| `S20`–`S2F` Old retrigger | `E9y` | covered |
| `S30`–`S3F` Vibrato waveform | `E4y` | covered |
| `S40`–`S4F` Tremolo waveform | `E7y` | covered |
| `SB0`–`SBF` Pattern loop | `E6y` | covered |
| `SC0`–`SCF` Note cut | `ECy` | covered |
| `SD0`–`SDF` Note delay | `EDy` | covered |
| `SE0`–`SEF` Pattern delay | `EEy` | covered |
| `SF0`–`SFF` Special event | `EFy` | covered |
| `T` Set Tempo | `Fxx` (xx≥32) | covered |
| `V` Set Global Volume | `Gxx` | covered |
| `W` Global VolSlide | `Hxx` | covered |
| `X` Set Panning (8-bit) | `8xx` | covered |
| ext 27 Set Volume | `Cxx` | covered (incl. vol-column re-encoding case) |
| ext 28 Key Off | `Kxx` | covered |
| ext 29 Set Env Position | `Lxx` | covered |
| ext 30 Tremor | `Txx` | covered |
| Unmapped MAS sub-commands (`S5y`–`SAy`) | (zeroed) | not exercised; no XM source produces these |
| MAS effects 13 (M), 14 (N), 25 (Y), 26 (Z) | (zeroed) | not exercised; IT-only |

### Known-loss categories (mas_spec.md Appendix C)

Each row corresponds to one category in
[`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas).
The "Classified by" column tells you exactly which branch of
`roundtrip_test.c` (or which other tool) catches it. If a category
shows up in a real failure and the listed branch isn't firing, that's
the place to start debugging.

| Category | Where in mas_spec | Classified by | Verdict |
|---|---|---|---|
| C.1 Stripped strings (title, names) | C.1 | `compare_header()` title/tracker branches | ✓ |
| C.2 Envelope data on disabled envelopes | C.2 | `compare_envelopes_and_vibrato()` "stripped" branches | ✓ |
| C.3 Sample alignment padding | C.3 | `compare_samples()` `+%d (alignment)` branch | ✓ |
| C.3 BIDI loop unroll (length doubled) | C.3 | `compare_samples()` `BIDI/loop unroll` branch | ✓ |
| C.3 16→8 bit downsample (GBA only) | C.3 | n/a (mas2xm rejects GBA) | ✗ |
| C.3 Loop unroll → cubic resample | C.3 | `compare_samples()` `cubic resample` branch | ✓ |
| C.3 Loop dropped entirely | C.3 | `compare_samples()` `loop dropped` branch | ✓ |
| C.3 Sign conversion ([-127, +127]) | C.3 | not detected (length-only check) | ~ |
| C.4 `E0y` filter zeroed | C.4 | `xm_effect_zeroed_by_mmutil()` | ✓ |
| C.4 `E3y` glissando control zeroed | C.4 | `xm_effect_zeroed_by_mmutil()` | ✓ |
| C.4 `E5y` set finetune zeroed | C.4 | `xm_effect_zeroed_by_mmutil()` | ✓ |
| C.4 Porta param ≥0xE0 → 0xDF | C.4 | `xm_porta_clamped()` | ✓ |
| C.5 Sample frequency `Hz/4` | C.5 | `compare_samples()` joint finetune+relnote | ✓ |
| C.5 Sample base rate `Hz·1024/32768` | C.5 | implicit in finetune+relnote check | ✓ |
| C.5 Fadeout `/32` rounding | C.5 | `compare_envelopes_and_vibrato()` `±31` branch | ✓ |
| C.5 Fadeout u8 cap (8160 ceiling) | C.5 (extension) | `compare_envelopes_and_vibrato()` cap branch | ✓ |
| C.5 Envelope X position drift | C.5 | `compare_envelopes_and_vibrato()` `±2` branch | ✓ |
| C.5 Auto-vibrato sweep (lossy) | C.5 | `compare_envelopes_and_vibrato()` "lossy u8" branch | ✓ |
| C.6 Pattern orders > 200 dropped | C.6 | `compare_header()` `clamped to 200` branch | ✓ |
| C.6 Channels > 32 | C.6 | n/a (XM also caps at 32) | ✓ |

### Pattern-diff categories (corpus-empirical)

These four categories surface when running the 46-XM corpus through
the mmutil → mas2xm pipeline. They are not byte-format losses (those
live in Appendix C above). They are *higher-level* tracker-semantics
losses that fall out of how mmutil compresses and orders pattern
data. The classifier in `roundtrip_test.c` recognizes each one and
marks it as expected.

| Category | Origin | Classified by | Verdict |
|---|---|---|---|
| 1. Unreferenced patterns (cache leak) | mmutil compression cache leaks across patterns that aren't in the order table | `!referenced[p]` + `entry_looks_like_cache_leak()` | ✓ |
| 2. Note-off + cached instrument | `MF_DVOL` cleared on note-off rows; instrument byte gets eaten by the value cache | `compare_patterns()` note-off branch | ✓ |
| 3. Effect zeroing | mmutil drops `E0y`, `E3y`, `E5y` (see Appendix C.4) | `xm_effect_zeroed_by_mmutil()` | ✓ |
| 4. Equivalent re-encoding (`E8y` → `8xx`) | XM has multiple ways to express set-panning; mas2xm picks one | `compare_patterns()` E8y branch | ✓ |
| 4. Set volume ↔ volume column equivalence | Same idea: `Cxx` vs vol column carry the same data | `compare_patterns()` Cxx branch | ✓ |
| 4. Porta clamp | mmutil clamps porta param ≥ 0xE0 to 0xDF (see Appendix C.4) | `xm_porta_clamped()` | ✓ |

### Out of scope (intentional gaps)

| Component | Why not covered |
|---|---|
| GBA sample format | mas2xm rejects GBA. Only NDS samples are read |
| MSL soundbank wrapper | mas2xm rejects MSL-dep modules. Samples must be inline |
| ADPCM samples | mas2xm doesn't decode IMA-ADPCM payloads |
| IT-mode pattern volume column | IT/S3M roundtrips are not validated by the test corpus (XM-only) |
| Pitch envelope (IT feature) | XM has no pitch envelope; the field is parsed but never compared |
| Sample PCM byte content | Roundtrip validates length / loop / format but not delta-encoding correctness on a byte-by-byte basis. A regression in `write_xm_sample_data()` that produces wrong audio but the right length would slip through. **Mitigation:** load the reconstructed XM in a tracker and listen. |
| Channel volume / panning per-channel arrays | Read into `Module` but never compared by `roundtrip_test`. Would surface only if a channel's data is silently corrupted in-place. |
| Performance / memory bounds | No benchmark, no memory ceiling, no fuzz target |

The "byte-by-byte sample content" gap is the most consequential. If
you suspect a delta-encoding regression, the easiest detector is the
manual one: convert two well-known XMs, listen to the recon in a
tracker, and watch for clicks at sample loop boundaries.

---

## Building the test tools

All test tools are standalone C files in `test/`. With the exception of
`check_vib.c` and `trace_write.c` (which link the mas2xm reader for
convenience), they have no dependencies beyond the C standard library
and can be compiled individually:

```bash
# Plain standalone tool
gcc -O2 -o test/full_cmp.exe       test/full_cmp.c

# Tools that link mas2xm sources
gcc -O2 -I. -o test/check_vib.exe   test/check_vib.c     src/mas_read.c src/io_util.c
gcc -O2 -I. -o test/trace_write.exe test/trace_write.c   src/mas_read.c src/io_util.c

# The one-for-all roundtrip test
gcc -O2 -o test/roundtrip_test.exe  test/roundtrip_test.c
```

The CMake build does **not** build these by default. They are
ad-hoc utilities, rebuilt as needed. The reference compiled binaries
already in `test/` (`*.exe`) were built with MinGW-w64 GCC.

---

## Known portability gotcha (Windows + MSYS bash + MinGW)

When the roundtrip test was first written it tried to silence mmutil's
chatty output by redirecting child stdout/stderr to `NUL` via
`freopen()` / `_dup2()` around `_spawnv()`. **This caused mmutil to hang
indefinitely** when launched from inside a MinGW-built executable that
was itself running under an MSYS bash with its own redirected stdout.
The file descriptor handed to the child was an MSYS pseudo-tty handle
that mmutil's MSVCRT couldn't write to without blocking.

The current code calls `_spawnv(_P_WAIT, prog, argv)` directly with no
fd manipulation. The child inherits the parent's stdio, which means
mmutil's `sample is at NN/NN of NN` progress lines are interleaved with
the test report. That noise is intentional. It's the price of avoiding
the hang. If you need a clean log, redirect the entire test output
externally (`./roundtrip_test.exe foo.xm > /tmp/foo.log 2>&1`) and grep
for the `RESULT:` line.

If you need to fix or extend the spawn logic, **never** wrap with cmd.exe
quoting tricks (`"\"prog\" \"arg\""`). Those have the same hanging
behavior under MSYS, just for different reasons. Stick to a direct
`_spawnv` / `execv` and accept the noise.

---

## 1. roundtrip_test.exe: One-for-all unit test

**Source:** [`test/roundtrip_test.c`](test/roundtrip_test.c)
**Usage:** `roundtrip_test.exe [options] input.xm`

The single test you should run after touching anything in `src/`. Given an
`.xm` input it performs the full pipeline in one process:

```
input.xm  →  mmutil  →  /tmp/<basename>.mas
                          │
                          ▼
                       mas2xm  →  /tmp/<basename>_recon.xm
                          │
                          ▼
                  in-process structural diff
                  classified into expected vs unexpected
                          │
                          ▼
              exit 0 (PASS) or exit 1 (FAIL)
```

The diff classifier embedded in `roundtrip_test.c` uses the same logic as
`full_cmp.c` but adds the **expected vs unexpected** distinction from
[`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas):

| Diff | Tolerance | Counted as |
|---|---|---|
| Module title / tracker name | always | expected (skipped) |
| Channel count rounded up to even | yes | expected |
| Sample length grew by ≤ 4 bytes | yes | expected (alignment) |
| Sample length grew by > 4 (BIDI unroll) | type byte changed `0x02 → 0x01` | expected |
| Loop start +1..+4 | yes | expected (start padding) |
| Loop length +2..+4 | yes | expected |
| Finetune ±3 | yes | expected (Hz/4 rounding) |
| Relative note ±1 | yes | expected |
| Pan byte difference ≤ 2 | yes | expected (>>1 / <<1 round trip) |
| Volume envelope X position ±1 per node | yes | expected |
| Fadeout ±31 | yes | expected (/32 rounding) |
| Vibrato sweep | yes | expected (lossy 8-bit truncate) |
| `E0y / E3y / E5y` zeroed | yes | expected (mmutil drops them) |
| Porta param ≥ `0xE0` clamped to `0xDF` | yes | expected |
| `E8y → 8xx` re-encoding | yes | expected (semantic equivalence) |
| Note-off + cached instrument lost | yes | expected (Errata D.1) |
| Pattern in unreferenced position | when not in order table | expected |
| Anything else | no | **UNEXPECTED, fails the test** |

**Options:**

```
roundtrip_test.exe [-v] [-k] [--mmutil PATH] [--mas2xm PATH] input.xm

  -v               Verbose: show every diff with classification.
  -k               Keep intermediate /tmp/<basename>.mas and _recon.xm files.
  --mmutil PATH    Override path to mmutil (default: $MMUTIL or
                   /c/devkitpro/tools/bin/mmutil.exe).
  --mas2xm PATH    Override path to mas2xm (default: ./build/mas2xm.exe or
                   ../build/mas2xm.exe).
```

**Output (PASS):**

```
roundtrip_test: input.xm
  [step 1/3] mmutil      ... ok    (3128 bytes -> 1496 bytes mas)
  [step 2/3] mas2xm      ... ok    (1496 bytes mas -> 3144 bytes xm)
  [step 3/3] structural diff
              header        : OK
              orders        : OK    (8 entries)
              patterns      : OK    (4 patterns, 0 unexpected diffs)
              samples       : OK    (3 samples, all diffs within tolerance)
              envelopes     : OK    (1 instrument)
              vibrato       : OK
              -----------------
              expected      : 7 diffs (sample length padding x2, finetune x3, ...)
              unexpected    : 0
  RESULT: PASS
```

**Output (FAIL):**

```
roundtrip_test: badinput.xm
  ...
  [step 3/3] structural diff
              patterns      : FAIL  (P2 R14 C5: note 33 vs 35)
              -----------------
              expected      : 4 diffs
              unexpected    : 1
  RESULT: FAIL
```

**Exit code:**

| Code | Meaning |
|---|---|
| 0 | PASS, all diffs accounted for by expected losses |
| 1 | FAIL: at least one unexpected diff, or one of the pipeline steps failed |
| 2 | usage error |

This is the test to wire up in CI / pre-commit / personal regression
checking.

---

## 2. full_cmp.exe: Structural XM-vs-XM comparator

**Source:** [`test/full_cmp.c`](test/full_cmp.c)
**Usage:** `full_cmp.exe original.xm reconstructed.xm`

The reference comparator. Reads two XM files into memory and reports
field-by-field differences in five categories. Used by both
`batch_test.sh` and (via the same logic) `roundtrip_test.exe`.

| Section | Compares |
|---|---|
| `=== HEADER ===` | `song_len`, `restart`, `channels`, `patterns`, `instruments`, `flags`, `speed`, `tempo`, title, tracker name, order table |
| `=== PATTERNS ===` | row count per pattern, then for each `(row, channel)` the decoded `(note, inst, vol, fx, param)` quintuple. Reports the first three diffs per pattern with `...+N more` for the rest. |
| `=== SAMPLES ===` | Per-instrument sample headers: `length`, `loop_start`, `loop_length`, `volume`, `finetune`, `type`, `panning`, `relative_note` |
| `=== ENVELOPES ===` | Volume and panning envelope point bytes (48 + 48), node counts, sustain/loop indices, envelope flags. Annotated counts and node lists shown when diffs exist. |
| `=== VIBRATO ===` | Per-instrument vibrato `(type, sweep, depth, rate)` and fadeout. |
| `=== SUMMARY ===` | One-line totals: pattern diffs, sample diffs, envelope diffs, vibrato diffs |

**Interpreting results:** see the table in §1 (the roundtrip test
classifier section). The TL;DR: *pattern diffs zero* means the music
roundtrips; sample / envelope / vibrato diffs are usually expected
losses documented in [`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas).

---

## 3. batch_test.sh: Corpus regression runner

**Source:** [`test/batch_test.sh`](test/batch_test.sh)
**Usage:** `bash test/batch_test.sh`

Walks `C:/Projects/MAXMXDS/songs/` recursively for every `*.xm`, runs the
full `xm → mas → xm → full_cmp` pipeline against each, and writes a
combined log to `test/batch_results.log`.

Per-file pipeline:

```
"$MMUTIL"   -d -m  $xm_file  -o $tmpdir/$idx_$name.mas
"$MAS2XM"        $tmpdir/$idx_$name.mas  $tmpdir/$idx_$name_recon.xm
"$FULL_CMP"      $xm_file                 $tmpdir/$idx_$name_recon.xm
```

The temp directory is created with `mktemp -d` and cleaned up by an
`EXIT` trap.

**Status counters and summary:**

```
========================================
         BATCH TEST SUMMARY
========================================
  Total XM files:           46
  mmutil failures:           0
  mas2xm failures:           0
  Files with pattern diffs: 14
  Clean pass (0 diffs):      1
  Other diffs (no patt):    31
========================================
```

The `Files with pattern diffs` count includes files with **expected**
pattern diffs from unreferenced patterns or note-off + instrument
suppression. Both are documented in
[`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas)
and classified as expected by `roundtrip_test`.

**Hardcoded paths** at the top of the script:

```bash
SONGS_DIR="/c/Projects/MAXMXDS/songs"
MMUTIL="/c/devkitpro/tools/bin/mmutil.exe"
MAS2XM="/c/Projects/mas2xm/build/mas2xm.exe"
FULL_CMP="/c/Projects/mas2xm/test/full_cmp.exe"
LOG="/c/Projects/mas2xm/test/batch_results.log"
```

Edit them if your layout differs.

---

## 4. debug_dump.exe: MAS pattern offset dumper

**Source:** [`test/debug_dump.c`](test/debug_dump.c)
**Usage:** `debug_dump.exe file.mas`

Bypasses the mas2xm reader and parses the MAS prefix and offset tables
directly. Prints:

- `inst_count`, `samp_count`, `patt_count`, `flags`
- The pattern offset table, both relative-to-base (`0x...`) and
  file-absolute (`file=0x...`).
- The first byte of pattern 0 (`row_count + 1` = number of rows).
- A 128-byte hex dump of pattern 0's body.

Use it to verify that mas2xm's reader is hitting the right file offsets,
or to spot mis-aligned offset tables in custom-built MAS files.

---

## 5. trace_patt.exe: MAS pattern data tracer

**Source:** [`test/trace_patt.c`](test/trace_patt.c)
**Usage:** `trace_patt.exe file.mas`

Manually decodes pattern 0 of a MAS file and prints the first 8 rows in
human-readable form:

```
Pattern 0: 64 rows
Row  0: ch1(m23 n61 i1 v40) ch3(m31 n0 i2 fx1/04) [end]
Row  1: ch1(m22 i1 v3a) [end]
...
```

This is the simplest place to validate the **MF carry-forward** logic
end-to-end. If the trace shows entries on row 0 but nothing on row 1+,
the file's value cache is suppressing values and you need MF handling.
The tool itself does **not** apply MF carry-forward. It shows only the
raw bytes that are physically present in the stream, so it serves as a
deliberate counter-test against `mas2xm`'s correct decoder.

---

## 6. check_xm.exe: XM pattern header inspector

**Source:** [`test/check_xm.c`](test/check_xm.c)
**Usage:** `check_xm.exe file.xm`

Reads an XM file and dumps:

- Header: `header_size`, `song_length`, `channels`, `patterns`, `instruments`
- The file offset where patterns start
- For each of the first three patterns: header length, packing type,
  row count, packed-data size, the first 32 bytes of packed data, and
  the decoded first row.

Use it after running `mas2xm` to make sure pattern headers are well
formed (`header_size = 9`, `packing = 0`) and that the decoded row 0
matches what you'd expect from the MAS file.

---

## 7. check_inst.exe: XM instrument/sample inspector (verbose)

**Source:** [`test/check_inst.c`](test/check_inst.c)
**Usage:** `check_inst.exe file.xm`

Walks an XM file's instrument chain. For each of the first five
instruments it prints:

- File offset, instrument-header size, sample count
- `sample_header_size` (should always be 40)
- `sample_map[0..11]`
- Volume envelope: first 3 nodes, point counts, vol/pan flags
- `fadeout`
- For up to 3 samples: `len`, `loop_start`, `loop_length`, `vol`,
  `finetune`, `type`, `pan`, `relnote`
- The total bytes of sample data following the headers

Use it to diagnose **instrument header size errors** (Errata D.2). If
mas2xm wrote `inst_size` wrong, the chain walks off-by-some bytes and
every subsequent instrument prints garbage.

---

## 8. check_inst2.exe: Compact XM inst/samp walker

**Source:** [`test/check_inst2.c`](test/check_inst2.c)
**Usage:** `check_inst2.exe file.xm`

A trimmed-down `check_inst` that prints all patterns' positions
beforehand and only the first three instruments after, with one line
per sample. Useful when you want a one-shot verification that the file
is parseable end-to-end without all the envelope detail.

---

## 9. check_samp.exe: MAS sample data inspector

**Source:** [`test/check_samp.c`](test/check_samp.c)
**Usage:** `check_samp.exe file.mas`

For up to the first five samples in a `.mas` file:

- File offset (both absolute and base-relative)
- Sample info: `default_vol`, `pan`, `freq` (Hz/4), vibrato fields,
  `gvol`, `vib_rate`, `msl_id`
- If `msl_id == 0xFFFF`, the inline NDS sample header:
  `loop_start` (words), `loop_length` (words), `format`, `repeat_mode`,
  `default_freq`, `point`
- Computed total payload size in words and bytes
- The first 16 bytes of raw sample data (signed PCM)

Use it to verify NDS sample header layout, especially the
words-vs-bytes loop fields, and to check that the runtime `point`
field is zero in the on-disk image.

---

## 10. check_all_samp.exe: Flat XM sample listing

**Source:** [`test/check_all_samp.c`](test/check_all_samp.c)
**Usage:** `check_all_samp.exe file.xm`

Walks the entire instrument chain and prints one line per sample with
`(inst#, samp#, length, loop_start, loop_length, volume, type, relative_note)`.
Faster than `check_inst` when you only care about sample headers and
want a flat sortable view (e.g., to diff with another build).

---

## 11. check_vib.exe: Vibrato value inspector

**Source:** [`test/check_vib.c`](test/check_vib.c)
**Usage:** Hard-coded to read `test/sweetdre.mas`.

Loads `test/sweetdre.mas` via the mas2xm library and prints any sample
that has non-zero vibrato fields:

```
Samp 3: type=0 depth=8 speed=4 rate=512
```

Used while diagnosing the lossy `vib_sweep = 32768/(sweep+1)` truncation.
Edit the source if you need a different input. It is intentionally
single-purpose and small.

**Build:**

```bash
gcc -I. -o test/check_vib.exe test/check_vib.c src/mas_read.c src/io_util.c
```

---

## 12. cmp_orders.exe: Header / order list compare

**Source:** [`test/cmp_orders.c`](test/cmp_orders.c)
**Usage:** Hard-coded paths (edit if needed).

Loads `C:/Projects/MAXMXDS/songs/BestOf/sweetdre.xm` and
`test/sweetdre_out.xm`, and prints a side-by-side line per file:

```
ORIGINAL: len=64 restart=0 ch=8 patt=21 inst=12 spd=6 tempo=125
  orders[0..20]: 0 1 2 3 4 ...
RECONSTR: len=64 restart=0 ch=8 patt=21 inst=12 spd=6 tempo=125
  orders[0..20]: 0 1 2 3 4 ...
```

Quick eyeball check that the song length, restart position, channel
count, and order list survived the roundtrip.

---

## 13. quick_orders.exe: Order dump for one XM

**Source:** [`test/quick_orders.c`](test/quick_orders.c)
**Usage:** Hard-coded to `/c/Projects/MAXMXDS/songs/XM/0-insnej.xm`.

Tiny one-shot: prints the song length and order table of a fixed file.
Used during debugging of the order-clamping logic; keep or rebuild to
target a different file.

---

## 14. cmp_patt.exe: Pattern row comparator

**Source:** [`test/cmp_patt.c`](test/cmp_patt.c)
**Usage:** Hard-coded paths (`sweetdre.xm` vs `sweetdre_out.xm`).

Decodes the first four rows of the first physical pattern of two XM
files and prints any non-empty channel entry as
`c<chan>[note,inst,vol,fx/param]`. Side-by-side dumps make it easy to
spot a single-bit mismatch like a wrong instrument number.

---

## 15. count_hdr.exe: XM instrument header size calculator

**Source:** [`test/count_hdr.c`](test/count_hdr.c)
**Usage:** `count_hdr.exe`

Prints the canonical XM instrument header size with and without samples:

```
XM instrument header size (with samples): 243
Without samples: 29
```

Pure documentation aid. Exists because Errata D.2 (off-by-one in the
243-byte header) has bitten more than one tracker implementation. The
file's inline comment lists every field and its byte count so you can
audit the calculation.

---

## 16. trace_write.exe: mas2xm reader output dumper

**Source:** [`test/trace_write.c`](test/trace_write.c)
**Usage:** Hard-coded to read `test/kuk.mas`.

Loads `test/kuk.mas` via the mas2xm library and prints:

- `inst_count`, `samp_count`
- For up to 5 samples: `data_length`, `loop_start`, `loop_length`,
  `format`, `repeat_mode`, and the malloc'd data pointer
- Instrument 1's note map entries 12..23 in hex

Used during development to confirm that `read_sample()` was producing
sane fields before `xm_write` consumed them. Edit the path or rebuild
against a different `.mas` to inspect a different file.

**Build:**

```bash
gcc -I. -o test/trace_write.exe test/trace_write.c src/mas_read.c src/io_util.c
```

---

## Pinned reference files

| File | Purpose |
|---|---|
| `test/kuk.mas` | Small XM compiled to MAS, used by `trace_write.c` and as a fast smoke test |
| `test/sweetdre.mas` | Larger module, used by `check_vib.c` and `cmp_*` tools |
| `test/kuk_out.xm` | Reference reconstruction of `kuk.mas` |
| `test/sweetdre_out.xm` | Reference reconstruction of `sweetdre.mas` |

Re-generate the `_out.xm` files with `mas2xm test/<name>.mas
test/<name>_out.xm` whenever the writer changes; commit the new outputs
to keep the regression baseline meaningful.

---

## Debugging a failing roundtrip

When `roundtrip_test` reports `RESULT: FAIL`, the goal is to figure out
whether the unexpected diff is **(a)** a real bug introduced into mas2xm
or **(b)** a genuinely-new "expected loss" category that the classifier
didn't know about. This section gives a step-by-step procedure that
assumes no prior context.

### Step 1: Get the full list of unexpected diffs

```bash
./test/roundtrip_test.exe -v -k input.xm 2>&1 | grep -E '\[UNEXPECTED\]|RESULT'
```

`-v` makes the test print every diff with its classification.
`-k` keeps the intermediate `tmp_mas` and `tmp_xm` files so you can
inspect them by hand. The paths are printed at the top of the report.

### Step 2: Categorize the failure

Match the first unexpected line against this table. Each entry tells you
which file in `mas2xm` to look at and what the typical fix is.

| Symptom | Most likely component | Where to look |
|---|---|---|
| `P<N> R<R> C<C>: [n.. i.. v.. fx../..] vs [n.. i.. v.. fx../..]` with non-empty recon | Pattern reader or effect mapper | `src/mas_read.c` `read_pattern()`, `src/effects.c` `mas_to_xm_effect()` |
| `P<N> R<R> C<C>` recon all zero (`n0 i0 v00 fx0/00`) | Pattern reader miss-handling MF carry-forward, OR a new unreferenced-pattern variant | `src/mas_read.c` lines ~262-303; check `referenced[]` logic in `roundtrip_test.c` |
| `I<N> S<M> length`/`loop_*` differing by an unexpected amount | Sample reader (word-vs-byte conversion) or sample writer | `src/mas_read.c` `read_sample()`, `src/xm_write.c` `write_xm_instrument()` sample header block |
| `I<N> S<M> finetune` / `relnote` joint diff > 4 | Frequency reconstruction (`freq_to_relnote_finetune()`) | `src/xm_write.c` ~62-86 |
| `I<N> nsamp X vs Y` (where Y < X) and Y > 0 | Likely orphan samples, already classified as expected. If still showing up, the note map is wrong | `src/mas_read.c` `read_instrument()` note-map handling |
| `I<N> vol/pan env count` mismatch with on-bit set in original | Envelope writer dropping data | `src/xm_write.c` ~265-330 |
| `I<N> vol env node[J] X/Y` mismatch | Envelope encoding precision OR a new format quirk | `src/mas_read.c` `read_envelope()`, mas_spec instruments.md §3 |
| `I<N> fadeout` mismatch outside ±31 and not 8160 cap | Fadeout scaling (mas writes /32, recon multiplies by 32) | `src/xm_write.c` line ~351 |
| `I<N> vibrato type/depth/rate` (NOT sweep) | Vibrato fields wired to the wrong sample, or auto-vibrato encoding wrong | `src/xm_write.c` ~333-348 |
| `channels: A vs B` outside expected rounding | `count_used_channels()` logic | `src/xm_write.c` ~21-46 |
| `song_len`/`tempo`/`speed`/`flags` mismatch | XM header writer | `src/xm_write.c` `xm_write()` ~432-500 |

### Step 3: Reproduce the diff in isolation

Once you have a guess at the failing component, reproduce the diff
without going through the full roundtrip.

```bash
# Examine the original XM
./test/check_xm.exe input.xm                  # patterns
./test/check_inst.exe input.xm                # instruments + envelopes
./test/check_all_samp.exe input.xm            # flat sample listing

# Examine the intermediate MAS (kept by -k above)
./test/debug_dump.exe /tmp/<basename>_rt.mas  # offsets + raw pattern 0
./test/check_samp.exe /tmp/<basename>_rt.mas  # MAS sample headers
./test/trace_patt.exe /tmp/<basename>_rt.mas  # pattern 0 raw decode

# Examine the reconstructed XM
./test/check_xm.exe /tmp/<basename>_rt_recon.xm
./test/check_inst.exe /tmp/<basename>_rt_recon.xm
./test/full_cmp.exe input.xm /tmp/<basename>_rt_recon.xm
```

`full_cmp` gives you a side-by-side report; the new test gives the
classification. Together they tell you both *what* is different and
*whether the difference is a known loss*.

### Step 4: Decide. Bug fix or classifier extension?

There are three possible verdicts:

1. **Bug in mas2xm.** The diff would not exist if mas2xm did the right
   thing. Fix the relevant `src/` file. The classifier should not be
   touched.
2. **Known loss the classifier didn't recognize.** The diff is one of
   the categories in
   [`mas_spec.md` Appendix C](../mas_spec/mas_spec.md#appendix-c-known-conversion-losses-xm-to-mas)
   but the comparator's tolerance was too tight (or the category was
   never coded). Add a new branch in `roundtrip_test.c`'s
   `compare_*` functions and document it in DOCUMENTATION.md §10.
3. **New category of loss.** The diff is an irreversible mmutil
   transform that nobody had documented before. Add it to
   `mas_spec.md` Appendix C and *then* extend the classifier.

The asymmetric rule of thumb:
> *Tighten the converter, loosen the classifier.* Whenever you can, fix
> the bug rather than widen the tolerance. Never widen the tolerance
> without first adding the explanation to mas_spec.md.

### Step 5: When mmutil itself is the variable

A small subset of files trigger mmutil bugs (sample fragmentation,
header rejection, ordering quirks). To rule mmutil out:

```bash
# Build the same .mas with a different mmutil version, if available:
"$MMUTIL_OLD" -d -m input.xm -ofile_old.mas
"$MMUTIL_NEW" -d -m input.xm -ofile_new.mas
cmp file_old.mas file_new.mas
```

If the two .mas files differ, the mas2xm output will too. But the
*classification* should still be the same. If a new mmutil produces a
different .mas that mas2xm can't classify, treat it as case (3) above.

### Step 6: Tolerance constants and where they come from

The numeric tolerance thresholds in `roundtrip_test.c` (top of the
file) come from `mas_spec.md` Appendix C. Each one corresponds to a
specific transform:

| Constant | Source | Value | Reason |
|---|---|---|---|
| `TOL_SAMPLE_LENGTH_PAD` | mas_spec C.3 (alignment) | 32 | Worst case is BIDI unroll + word align ≈ +24 |
| `TOL_LOOP_START_PAD` | mas_spec C.3 (start padding) | 8 | NDS 8-bit needs `loop_start % 4 == 0` |
| `TOL_LOOP_LENGTH_PAD` | mas_spec C.3 (loop unroll) | 32 | Loop unroll repeats up to 1024 bytes worth |
| `TOL_FINETUNE` | mas_spec C.5 | 4 | `freq/4` rounding cascades through `freq_to_relnote_finetune` |
| `TOL_RELNOTE` | mas_spec C.5 | 1 | Complementary to finetune |
| `TOL_PAN` | xm_write panning encoding | 2 | `(xm_pan & 0xFE) >> 1 << 1` round-trip loses bit 0 |
| `TOL_FADEOUT` | mas_spec C.5 (`/32`) | 32 | Quantization step |
| `TOL_ENV_X` | mas_spec C.5 (envelope) | 2 | `range` clamped to 511; integer `((next_y - base) * 512) / range` |

If you need to widen one of these, search the code for the constant
and update both the value and the table comment.

---

## Workflow recipes

**"I just modified `mas_read.c` / `xm_write.c` / `effects.c`. Did I break
anything?"**

```bash
# Build
cmake --build build

# Smoke test on the pinned reference files
./build/mas2xm.exe test/kuk.mas       /tmp/kuk_out.xm
./build/mas2xm.exe test/sweetdre.mas  /tmp/sweetdre_out.xm

# One-for-all roundtrip on each
./test/roundtrip_test.exe -v test/kuk.mas       || echo "kuk failed"
./test/roundtrip_test.exe -v test/sweetdre.mas  || echo "sweetdre failed"

# Full corpus
bash test/batch_test.sh
diff <(grep CLEAN test/batch_results.log) /tmp/old_clean   # optional
```

**"Pattern diff in one specific file. What is it?"**

```bash
# Get a structured report
./test/full_cmp.exe orig.xm /tmp/recon.xm | less

# If the diff is in pattern N row R channel C, look at the raw MAS:
./test/debug_dump.exe /tmp/orig.mas       # find pattern N's offset
./test/trace_patt.exe /tmp/orig.mas       # decode patt 0 manually

# And inspect the reconstructed XM patt header to make sure it parses:
./test/check_xm.exe /tmp/recon.xm
```

**"Sample data looks wrong."**

```bash
./test/check_samp.exe /tmp/orig.mas         # MAS-side sample header
./test/check_all_samp.exe /tmp/recon.xm     # XM-side sample headers
./test/check_inst.exe /tmp/recon.xm         # Full instrument+sample chain
```
