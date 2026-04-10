# mas2xm

**A MAS → XM converter. The inverse of `mmutil`.**

`mas2xm` reads a [maxmod](https://maxmod.devkitpro.org/) `.mas` module file and
reconstructs a FastTracker II `.xm` file from it. It is the deliberate inverse
of mmutil's XM → MAS path: every transformation that mmutil applies in
`Load_XM` / `CONV_XM_EFFECT` / `Write_Sample` is reversed here.

| Reads | Writes |
|---|---|
| `.mas` song module (NDS sample format, `MAS_VERSION = 0x18`) | `.xm` (FastTracker II Extended Module, version `0x0104`) |

## Why

`.mas` is the binary module format consumed by maxmod on the NDS/GBA. It is
normally a one-way destination: you ship `.mas` files with your ROM and the
original `.xm` sources live separately. `mas2xm` lets you go the other way —
useful for recovering tracker sources from a shipped ROM, auditing what
mmutil actually produced, or round-tripping a module through the toolchain
to verify conversion fidelity.

## Building

C99, CMake, no external dependencies beyond the C standard library.

```bash
cmake -S . -B build
cmake --build build
```

The result is `build/mas2xm` (or `build/mas2xm.exe` on Windows). Tested with
GCC (MSYS2/MinGW-w64) and MSVC.

## Usage

```
mas2xm v0.1.0 - MAS to XM converter
Usage: mas2xm [-v] input.mas output.xm

Options:
  -v    Verbose output (dumps instruments, samples, patterns as they load)
```

Example:

```bash
./build/mas2xm song.mas song_recon.xm
./build/mas2xm -v song.mas song_recon.xm
```

Exit code is `0` on success, `1` on any failure (bad input, parse error,
unwritable output).

## Scope

**In scope:**

- Song-type `.mas` files (`type == 0`) from mmutil with embedded NDS samples.
- Both XM-mode and IT-mode MAS flags during decoding (XM-mode is the
  validated path).
- Envelopes, note maps, auto-vibrato, and the full MAS effect command set
  that has an XM equivalent.

**Out of scope:**

- Standalone-sample `.mas` files (`type == 1` or `2`).
- GBA-format MAS samples (8-bit unsigned PCM — the sample header layout
  differs).
- MSL soundbank containers (`*maxmod*`). Samples must be embedded
  (`msl_id == 0xFFFF`).
- IT/S3M/MOD → MAS → XM round-trips. mmutil can take those as input, but
  IT-specific volume-column and effect commands cannot losslessly carry
  into XM.

## Lossy paths

A few conversions are structurally lossy. See
[`DOC.md` §10](DOC.md#10-known-conversion-losses) for the full list — the
main ones are:

- Envelope node X positions are reconstructed from the delta/base/range
  encoding; the original `delta` byte is discarded.
- IT-only effects with no XM equivalent (e.g. some Sxx subcommands) are
  dropped or mapped to the closest XM command.
- Sample loop fields are rebuilt from the NDS word-count convention.

## Project layout

```
mas2xm/
├── CMakeLists.txt
├── DOC.md              Full technical documentation
├── TEST_TOOLS.md       Reference for everything in test/
├── ACKNOWLEDGMENTS.md
├── LICENSE             MIT
├── src/
│   ├── main.c          CLI front-end
│   ├── types.h         Module / Pattern / Sample / Instrument structs
│   ├── io_util.c/.h    Little-endian read/write helpers
│   ├── mas_read.c/.h   .mas binary → in-memory Module
│   ├── effects.c/.h    MAS (IT-letter) → XM numeric effect mapper
│   └── xm_write.c/.h   in-memory Module → .xm binary
└── test/
    ├── full_cmp.c      Structural XM-vs-XM comparator
    ├── roundtrip_test.c  XM → MAS → XM unit test
    ├── batch_test.sh   Orchestrates mmutil + mas2xm + full_cmp over a corpus
    └── ...             (see TEST_TOOLS.md)
```

For the full conversion pipeline, data model, reader/writer internals, and
reverse effect mapping, see [`DOC.md`](DOC.md).

## Testing

The `test/` directory contains a roundtrip harness and several inspection
tools. The canonical check is:

```bash
# XM → mmutil → .mas → mas2xm → .xm → compare against original
./test/batch_test.sh path/to/xm_corpus
```

See [`TEST_TOOLS.md`](TEST_TOOLS.md) for what each tool does and how to
invoke it.

## License

MIT. See [`LICENSE`](LICENSE).

`mas2xm` is an independent reimplementation of the inverse of mmutil's XM
loader. It is not affiliated with or endorsed by the maxmod project. See
[`ACKNOWLEDGMENTS.md`](ACKNOWLEDGMENTS.md) for credits and references.
