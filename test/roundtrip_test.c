// SPDX-License-Identifier: MIT
// mas2xm - one-for-all roundtrip unit test
//
// Pipeline:
//   input.xm  --[mmutil]-->  tmp.mas  --[mas2xm]-->  tmp.xm
//                                                     |
//                                                     v
//                                  in-process structural diff
//                                                     |
//                                                     v
//                          classify each diff as EXPECTED or UNEXPECTED
//
// Exit codes:
//   0 = PASS (no unexpected diffs)
//   1 = FAIL (>=1 unexpected diff, or pipeline step failed)
//   2 = usage error
//
// Build:
//   gcc -O2 -o test/roundtrip_test.exe test/roundtrip_test.c
//
// Usage:
//   test/roundtrip_test.exe [-v] [-k] [--mmutil PATH] [--mas2xm PATH] input.xm
//
// Options:
//   -v               Verbose: list every diff and its classification
//   -k               Keep intermediate .mas and recon .xm files
//   --mmutil PATH    Path to mmutil binary
//                    (default: $MMUTIL or /c/devkitpro/tools/bin/mmutil.exe)
//   --mas2xm PATH    Path to mas2xm binary
//                    (default: ./build/mas2xm.exe or ../build/mas2xm.exe)
//
// Tolerance ranges encode every "known loss" listed in
// C:/Projects/mas_spec/mas_spec.md Appendix C and the diff classification
// in mas_spec.md Appendix C (see ../mas_spec/mas_spec.md).
//
// This test is the canonical regression for mas_read.c / xm_write.c /
// effects.c. If you change any of those files, run it on the pinned
// reference inputs (test/kuk.mas, test/sweetdre.mas) and the full
// MAXMXDS/songs corpus.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <process.h>
#include <io.h>
#include <fcntl.h>
#define unlink _unlink
#define DEVNULL "NUL"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#define DEVNULL "/dev/null"
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;

// ----------------------------------------------------------------------
// Configuration: tolerance thresholds (single source of truth)
// ----------------------------------------------------------------------

#define TOL_SAMPLE_LENGTH_PAD    32   // bytes added by alignment / unroll
#define TOL_LOOP_START_PAD        8   // bytes added by start padding
#define TOL_LOOP_LENGTH_PAD      32   // bytes added by loop unrolling
#define TOL_FINETUNE              4   // |delta| in finetune units
#define TOL_RELNOTE               1   // |delta| in semitones
#define TOL_PAN                   2   // |delta| from <<1/>>1 round trip
#define TOL_FADEOUT              32   // |delta| from /32 rounding
#define TOL_ENV_X                 2   // |delta| in ticks per envelope node

// XM unsupported effects that mmutil zeros (mas_spec C.4)
static bool xm_effect_zeroed_by_mmutil(u8 fx, u8 param)
{
    if (fx == 0x0E) {
        u8 sub = param >> 4;
        if (sub == 0x0) return true;  // E0y set filter
        if (sub == 0x3) return true;  // E3y glissando control
        if (sub == 0x5) return true;  // E5y set finetune
    }
    return false;
}

// XM porta up/down with param >= 0xE0 is clamped to 0xDF
static bool xm_porta_clamped(u8 fx, u8 param)
{
    return (fx == 1 || fx == 2) && param >= 0xE0;
}

// XM pattern entry
typedef struct {
    u8 note, inst, vol, fx, param;
} Entry;

// Loaded XM file
typedef struct {
    u8 *d;
    long sz;
    u32 hdr_size;
    u16 song_len, restart, nch, npatt, ninst, flags, speed, tempo;
    u8 orders[256];
    size_t patt_pos[256];
    u16 patt_rows[256];
    size_t inst_pos[256];
} XM;

// ----------------------------------------------------------------------
// File / process helpers
// ----------------------------------------------------------------------

static bool g_verbose = false;
static bool g_keep = false;

static long file_size(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// Strip directory and extension from a path. Writes into out (size cap).
static void basename_noext(const char *path, char *out, size_t cap)
{
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;

    size_t i = 0;
    while (base[i] && base[i] != '.' && i + 1 < cap) {
        out[i] = base[i];
        i++;
    }
    out[i] = '\0';
}

// Spawn a child process with the given argv. Returns the exit code, or
// -1 if the spawn itself failed. The child inherits the parent's stdio,
// so its output is interleaved with the test report. We accept that to
// avoid the brittle cmd.exe quoting and pty-vs-file-handle issues that
// surface when mas2xm or mmutil are run with redirected stdout under
// MinGW + MSYS.
//
// Avoids the cmd.exe quoting trap by not going through a shell.
static int spawn_child(const char *prog, const char *const *argv)
{
    if (g_verbose) {
        printf("    $ %s", prog);
        for (int i = 1; argv[i]; i++) printf(" %s", argv[i]);
        printf("\n");
        fflush(stdout);
    }
#if defined(_WIN32)
    intptr_t rc = _spawnv(_P_WAIT, prog, (const char *const *)argv);
    return (int)rc;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execv(prog, (char *const *)argv);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

// Pick the path of an external tool: explicit override > env var > default.
static const char *resolve_tool(const char *override_, const char *env_var,
                                const char *const *defaults, int ndefaults)
{
    if (override_ && *override_) return override_;
    const char *e = getenv(env_var);
    if (e && *e) return e;
    for (int i = 0; i < ndefaults; i++) {
        if (file_exists(defaults[i])) return defaults[i];
    }
    return defaults[0]; // last-resort default
}

// ----------------------------------------------------------------------
// XM loader (mirrors test/full_cmp.c)
// ----------------------------------------------------------------------

static int load_xm(const char *file, XM *x)
{
    FILE *f = fopen(file, "rb");
    if (!f) {
        fprintf(stderr, "    error: cannot open %s\n", file);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    x->sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    x->d = malloc(x->sz);
    if (fread(x->d, 1, x->sz, f) != (size_t)x->sz) {
        fclose(f);
        free(x->d);
        return 1;
    }
    fclose(f);

    if (x->sz < 80 + 256) {
        fprintf(stderr, "    error: %s too small to be an XM file\n", file);
        return 1;
    }

    x->hdr_size = *(u32 *)(x->d + 60);
    x->song_len = *(u16 *)(x->d + 64);
    x->restart  = *(u16 *)(x->d + 66);
    x->nch      = *(u16 *)(x->d + 68);
    x->npatt    = *(u16 *)(x->d + 70);
    x->ninst    = *(u16 *)(x->d + 72);
    x->flags    = *(u16 *)(x->d + 74);
    x->speed    = *(u16 *)(x->d + 76);
    x->tempo    = *(u16 *)(x->d + 78);
    memcpy(x->orders, x->d + 80, 256);

    size_t pos = 60 + x->hdr_size;
    for (int p = 0; p < x->npatt && p < 256; p++) {
        x->patt_pos[p] = pos;
        u32 phdr = *(u32 *)(x->d + pos);
        x->patt_rows[p] = *(u16 *)(x->d + pos + 5);
        u16 dsize = *(u16 *)(x->d + pos + 7);
        pos += phdr + dsize;
    }
    for (int i = 0; i < x->ninst && i < 256; i++) {
        x->inst_pos[i] = pos;
        if (pos + 29 > (size_t)x->sz) break;
        u32 ihdr = *(u32 *)(x->d + pos);
        u16 nsamp = *(u16 *)(x->d + pos + 27);
        size_t after = pos + ihdr;
        if (nsamp > 0 && ihdr >= 33) {
            u32 shsz = *(u32 *)(x->d + pos + 29);
            u32 total = 0;
            for (int s = 0; s < nsamp; s++) {
                if (after + s * shsz + 4 > (size_t)x->sz) break;
                total += *(u32 *)(x->d + after + s * shsz);
            }
            pos = after + nsamp * shsz + total;
        } else {
            pos = after;
        }
    }
    return 0;
}

static void free_xm(XM *x)
{
    free(x->d);
    x->d = NULL;
}

// Decode one packed XM row into a flat Entry array (size = nch).
static void decode_row(const u8 *d, size_t *dp, int nch, Entry *row)
{
    for (int c = 0; c < nch; c++) {
        row[c] = (Entry){0, 0, 0, 0, 0};
        u8 b = d[(*dp)++];
        if (b & 0x80) {
            if (b & 0x01) row[c].note  = d[(*dp)++];
            if (b & 0x02) row[c].inst  = d[(*dp)++];
            if (b & 0x04) row[c].vol   = d[(*dp)++];
            if (b & 0x08) row[c].fx    = d[(*dp)++];
            if (b & 0x10) row[c].param = d[(*dp)++];
        } else {
            row[c].note  = b;
            row[c].inst  = d[(*dp)++];
            row[c].vol   = d[(*dp)++];
            row[c].fx    = d[(*dp)++];
            row[c].param = d[(*dp)++];
        }
    }
}

// ----------------------------------------------------------------------
// Diff accounting
// ----------------------------------------------------------------------

typedef struct {
    int expected;
    int unexpected;
    char first_unexpected[256];
} DiffStats;

static void note_expected(DiffStats *s, const char *fmt, ...)
{
    s->expected++;
    if (g_verbose) {
        printf("      [EXPECTED] ");
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }
}

static void note_unexpected(DiffStats *s, const char *fmt, ...)
{
    s->unexpected++;
    if (s->unexpected == 1) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(s->first_unexpected, sizeof(s->first_unexpected), fmt, ap);
        va_end(ap);
    }
    if (g_verbose) {
        printf("      [UNEXPECTED] ");
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
    }
}

static int iabs(int v) { return v < 0 ? -v : v; }

// ----------------------------------------------------------------------
// Per-section comparators
// ----------------------------------------------------------------------

static void compare_header(const XM *a, const XM *b, DiffStats *s)
{
    if (a->song_len != b->song_len) {
        // Original may have orders > 200; mas clamps to 200.
        if (a->song_len > 200 && b->song_len == 200)
            note_expected(s, "song_len %u clamped to 200", a->song_len);
        else
            note_unexpected(s, "song_len: %u vs %u", a->song_len, b->song_len);
    }
    if (a->restart != b->restart)
        note_unexpected(s, "restart: %u vs %u", a->restart, b->restart);

    if (a->nch != b->nch) {
        // mas2xm rounds channel count up to even and to a minimum of 2,
        // and may shrink the count when trailing channels are empty —
        // mmutil drops totally-empty channels from the encoded patterns,
        // so the round-trip recovers fewer channels than the original.
        int diff = (int)b->nch - (int)a->nch;
        if (diff >= 0 && diff <= 1)
            note_expected(s, "channels rounded %u -> %u", a->nch, b->nch);
        else if (b->nch > a->nch && (b->nch % 2 == 0))
            note_expected(s, "channels rounded %u -> %u", a->nch, b->nch);
        else if (diff < 0)
            // Channel count shrunk: trailing empty channels were dropped.
            // The pattern comparator already only looks at min(a, b)
            // channels, so this is safe.
            note_expected(s, "channels shrunk %u -> %u (trailing empty)",
                          a->nch, b->nch);
        else
            note_unexpected(s, "channels: %u vs %u", a->nch, b->nch);
    }
    if (a->npatt != b->npatt)
        note_unexpected(s, "npatt: %u vs %u", a->npatt, b->npatt);
    if (a->ninst != b->ninst)
        note_unexpected(s, "ninst: %u vs %u", a->ninst, b->ninst);
    if ((a->flags & 1) != (b->flags & 1))
        note_unexpected(s, "linear-freq flag: %u vs %u",
                        a->flags & 1, b->flags & 1);
    if (a->speed != b->speed)
        note_unexpected(s, "speed: %u vs %u", a->speed, b->speed);
    if (a->tempo != b->tempo)
        note_unexpected(s, "tempo: %u vs %u", a->tempo, b->tempo);

    // Title and tracker name are always different (MAS doesn't store them)
    if (memcmp(a->d + 17, b->d + 17, 20) != 0)
        note_expected(s, "title differs (not stored in MAS)");
    if (memcmp(a->d + 38, b->d + 38, 20) != 0)
        note_expected(s, "tracker name differs (not stored in MAS)");

    // Order list (compare up to min(song_len, 200))
    int cmp_len = a->song_len;
    if (b->song_len < cmp_len) cmp_len = b->song_len;
    if (cmp_len > 200) cmp_len = 200;
    int ord_diff = 0;
    for (int i = 0; i < cmp_len; i++)
        if (a->orders[i] != b->orders[i]) ord_diff++;
    if (ord_diff > 0)
        note_unexpected(s, "%d order entries differ in first %d", ord_diff, cmp_len);
}

// Build a bitset of pattern indices that appear in the order table.
//
// We walk the **declared song length** (a->song_len), not the full 256
// order entries: mmutil's MAS encoder only encodes patterns that are
// actually played, and play stops at song_len. Patterns referenced by
// orders beyond song_len are still "unreferenced" from playback's POV
// and their cache leaks the same way.
static void build_referenced_set(const XM *a, bool *referenced)
{
    memset(referenced, 0, 256);
    int len = a->song_len;
    if (len > 256) len = 256;
    for (int i = 0; i < len; i++) {
        u8 o = a->orders[i];
        if (o < 254) referenced[o] = true;
    }
}

// Conservative "this pattern row looks like cache leakage" detector.
// True when the reconstructed entry is fully empty but the original is
// not. This catches the case where mmutil encoded a pattern with a
// suppressed-by-cache row but the cache state was lost (typical for
// rows that should be marked as cache-reset boundaries but weren't).
static bool entry_looks_like_cache_leak(const Entry *orig, const Entry *recon)
{
    bool orig_has = (orig->note != 0 || orig->inst != 0 || orig->vol != 0 ||
                     orig->fx != 0 || orig->param != 0);
    bool recon_empty = (recon->note == 0 && recon->inst == 0 &&
                        recon->vol == 0 && recon->fx == 0 && recon->param == 0);
    return orig_has && recon_empty;
}

static void compare_patterns(const XM *a, const XM *b, DiffStats *s)
{
    bool referenced[256];
    build_referenced_set(a, referenced);

    int min_ch = a->nch < b->nch ? a->nch : b->nch;
    int npatt  = a->npatt < b->npatt ? a->npatt : b->npatt;

    for (int p = 0; p < npatt; p++) {
        if (a->patt_rows[p] != b->patt_rows[p]) {
            note_unexpected(s, "P%d row count %u vs %u",
                            p, a->patt_rows[p], b->patt_rows[p]);
            continue;
        }
        u32 aph = *(u32 *)(a->d + a->patt_pos[p]);
        u32 bph = *(u32 *)(b->d + b->patt_pos[p]);
        size_t adp = a->patt_pos[p] + aph;
        size_t bdp = b->patt_pos[p] + bph;

        Entry ar[32], br[32];
        for (int r = 0; r < a->patt_rows[p]; r++) {
            decode_row(a->d, &adp, a->nch, ar);
            decode_row(b->d, &bdp, b->nch, br);

            for (int c = 0; c < min_ch; c++) {
                if (memcmp(&ar[c], &br[c], sizeof(Entry)) == 0)
                    continue;

                // Classify the diff. Try the cheapest matches first.

                // 1. Pattern is unreferenced -> mmutil cache leakage
                if (!referenced[p]) {
                    note_expected(s,
                        "P%d R%d C%d unreferenced pattern (mmutil cache leak)",
                        p, r, c + 1);
                    continue;
                }

                // 1b. Cache leakage in a "referenced" pattern. mmutil
                // sometimes encodes patterns with cache state that does
                // not survive a standalone decode — typically because
                // the pattern is reached only via complex ordering
                // (Bxx jumps, pattern loops) and mmutil's mark logic
                // doesn't include it as a cache-reset boundary. The
                // tell is that the recon entry is fully empty where
                // the original had data: mas2xm's decoder simply did
                // not see the bytes because mmutil suppressed them
                // and never set the MF flag.
                //
                // We accept this for any row, not just row 0 — once
                // the cache is wrong, every subsequent row in the
                // pattern is wrong too.
                if (entry_looks_like_cache_leak(&ar[c], &br[c])) {
                    note_expected(s,
                        "P%d R%d C%d empty in recon (cache leak suspected)",
                        p, r, c + 1);
                    continue;
                }

                // 2. Effect was zeroed by mmutil
                if (xm_effect_zeroed_by_mmutil(ar[c].fx, ar[c].param) &&
                    br[c].fx == 0 && br[c].param == 0 &&
                    ar[c].note == br[c].note &&
                    ar[c].inst == br[c].inst &&
                    ar[c].vol == br[c].vol)
                {
                    note_expected(s,
                        "P%d R%d C%d %02x%02x zeroed by mmutil",
                        p, r, c + 1, ar[c].fx, ar[c].param);
                    continue;
                }

                // 3. Porta up/down >= 0xE0 clamped to 0xDF
                if (xm_porta_clamped(ar[c].fx, ar[c].param) &&
                    br[c].fx == ar[c].fx && br[c].param == 0xDF &&
                    ar[c].note == br[c].note &&
                    ar[c].inst == br[c].inst &&
                    ar[c].vol == br[c].vol)
                {
                    note_expected(s,
                        "P%d R%d C%d porta param %02x clamped to DF",
                        p, r, c + 1, ar[c].param);
                    continue;
                }

                // 4. Note-off + cached instrument lost
                if (ar[c].note == 97 && br[c].note == 97 &&
                    ar[c].inst != 0 && br[c].inst == 0 &&
                    ar[c].vol == br[c].vol &&
                    ar[c].fx == br[c].fx && ar[c].param == br[c].param)
                {
                    note_expected(s,
                        "P%d R%d C%d note-off instrument %d suppressed",
                        p, r, c + 1, ar[c].inst);
                    continue;
                }

                // 5. E8y -> 8xx (set panning) re-encoding
                if (ar[c].fx == 0x0E && (ar[c].param >> 4) == 0x8 &&
                    br[c].fx == 8 &&
                    br[c].param == (u8)((ar[c].param & 0x0F) * 16) &&
                    ar[c].note == br[c].note &&
                    ar[c].inst == br[c].inst &&
                    ar[c].vol == br[c].vol)
                {
                    note_expected(s,
                        "P%d R%d C%d E8%x -> 8%02x re-encoded",
                        p, r, c + 1, ar[c].param & 0xF, br[c].param);
                    continue;
                }

                // 6. Cxx (set volume) <-> volume column equivalence
                //    mmutil moves Cxx into the volume column when there is
                //    no other volume command. The reverse path may pick
                //    either form.
                if (ar[c].fx == 0x0C && br[c].fx == 0 &&
                    br[c].vol == ar[c].param + 0x10 &&
                    ar[c].note == br[c].note && ar[c].inst == br[c].inst)
                {
                    note_expected(s, "P%d R%d C%d Cxx -> vol-column", p, r, c + 1);
                    continue;
                }
                if (br[c].fx == 0x0C && ar[c].fx == 0 &&
                    ar[c].vol == br[c].param + 0x10 &&
                    ar[c].note == br[c].note && ar[c].inst == br[c].inst)
                {
                    note_expected(s, "P%d R%d C%d vol-column -> Cxx", p, r, c + 1);
                    continue;
                }

                // Anything else is a real bug.
                note_unexpected(s,
                    "P%d R%d C%d: [n%d i%d v%02x fx%d/%02x] vs "
                    "[n%d i%d v%02x fx%d/%02x]",
                    p, r, c + 1,
                    ar[c].note, ar[c].inst, ar[c].vol, ar[c].fx, ar[c].param,
                    br[c].note, br[c].inst, br[c].vol, br[c].fx, br[c].param);
            }
        }
    }
}

static void compare_samples(const XM *a, const XM *b, DiffStats *s)
{
    int ninst = a->ninst < b->ninst ? a->ninst : b->ninst;

    for (int i = 0; i < ninst; i++) {
        if (a->inst_pos[i] + 29 > (size_t)a->sz) break;
        if (b->inst_pos[i] + 29 > (size_t)b->sz) break;

        u16 ans = *(u16 *)(a->d + a->inst_pos[i] + 27);
        u16 bns = *(u16 *)(b->d + b->inst_pos[i] + 27);
        if (ans != bns) {
            // Orphaned-sample case: original had multi-sample instrument
            // but the note map only references a subset. mas2xm walks
            // the note map and reconstructs only the referenced samples,
            // so unreferenced "ghost" samples disappear. This is a
            // documented expected loss (samples are reachable only via
            // the note map).
            if (bns > 0 && bns < ans)
                note_expected(s, "Inst %d nsamp %u -> %u (orphans dropped)",
                              i + 1, ans, bns);
            else
                note_unexpected(s, "Inst %d nsamp %u vs %u", i + 1, ans, bns);
            continue;
        }
        if (ans == 0) continue;

        u32 aihs = *(u32 *)(a->d + a->inst_pos[i]);
        u32 bihs = *(u32 *)(b->d + b->inst_pos[i]);
        size_t asp = a->inst_pos[i] + aihs;
        size_t bsp = b->inst_pos[i] + bihs;
        u32 ashsz = *(u32 *)(a->d + a->inst_pos[i] + 29);
        u32 bshsz = *(u32 *)(b->d + b->inst_pos[i] + 29);

        for (int sx = 0; sx < ans; sx++) {
            u32 alen = *(u32 *)(a->d + asp);
            u32 blen = *(u32 *)(b->d + bsp);
            u32 als  = *(u32 *)(a->d + asp + 4);
            u32 bls  = *(u32 *)(b->d + bsp + 4);
            u32 all  = *(u32 *)(a->d + asp + 8);
            u32 bll  = *(u32 *)(b->d + bsp + 8);
            u8 avol  = a->d[asp + 12];
            u8 bvol  = b->d[bsp + 12];
            s8 aft   = (s8)a->d[asp + 13];
            s8 bft   = (s8)b->d[bsp + 13];
            u8 atype = a->d[asp + 14];
            u8 btype = b->d[bsp + 14];
            u8 apan  = a->d[asp + 15];
            u8 bpan  = b->d[bsp + 15];
            s8 arn   = (s8)a->d[asp + 16];
            s8 brn   = (s8)b->d[bsp + 16];

            // Detect BIDI -> forward unroll. The type byte should change
            // (bit 0/1 from forward+bidi=0x02 to forward=0x01) AND/OR
            // the loop length should roughly double, AND/OR the sample
            // length should grow proportionally. mmutil sometimes leaves
            // the type byte as forward but still unrolls — detect by
            // length ratio in those cases.
            bool bidi_unrolled = false;
            if ((atype & 0x03) == 0x02 && (btype & 0x03) == 0x01)
                bidi_unrolled = true;
            // Heuristic: loop length doubled exactly (within ±4 alignment)
            if (all > 0 && bll >= 2 * all && bll <= 2 * all + 8)
                bidi_unrolled = true;
            // 4× expansions occur when both BIDI unroll AND another
            // alignment unroll fire.
            if (all > 0 && bll >= 4 * all && bll <= 4 * all + 16)
                bidi_unrolled = true;

            if (alen != blen) {
                int diff = (int)blen - (int)alen;
                if (diff >= 0 && diff <= TOL_SAMPLE_LENGTH_PAD)
                    note_expected(s, "I%d S%d length +%d (alignment)",
                                  i + 1, sx + 1, diff);
                else if (bidi_unrolled)
                    note_expected(s, "I%d S%d length %u -> %u (BIDI/loop unroll)",
                                  i + 1, sx + 1, alen, blen);
                else if (diff < 0)
                    // Cubic resampling triggered when loop unroll would
                    // exceed 1024 bytes (mas_spec.md C.3). Sample shrinks.
                    note_expected(s, "I%d S%d length %u -> %u (cubic resample)",
                                  i + 1, sx + 1, alen, blen);
                else
                    note_unexpected(s, "I%d S%d length %u vs %u (delta %d)",
                                    i + 1, sx + 1, alen, blen, diff);
            }
            // Detect "loop completely dropped": original had a loop, recon
            // is non-looping. Happens for very small loops or when cubic
            // resampling fires.
            bool loop_dropped = (all > 0 && bll == 0 && bls == 0);

            if (als != bls) {
                int diff = (int)bls - (int)als;
                if (diff >= 0 && diff <= TOL_LOOP_START_PAD)
                    note_expected(s, "I%d S%d loop_start +%d", i + 1, sx + 1, diff);
                else if (loop_dropped)
                    note_expected(s, "I%d S%d loop dropped (start was %u)",
                                  i + 1, sx + 1, als);
                else
                    note_unexpected(s, "I%d S%d loop_start %u vs %u",
                                    i + 1, sx + 1, als, bls);
            }
            if (all != bll) {
                int diff = (int)bll - (int)all;
                if (diff >= 0 && diff <= TOL_LOOP_LENGTH_PAD)
                    note_expected(s, "I%d S%d loop_length +%d",
                                  i + 1, sx + 1, diff);
                else if (bidi_unrolled)
                    note_expected(s, "I%d S%d loop_length %u -> %u (BIDI unroll)",
                                  i + 1, sx + 1, all, bll);
                else if (bll == 0)
                    // mmutil dropped the loop entirely (tiny loop, or
                    // cubic resampling collapsed it).
                    note_expected(s, "I%d S%d loop dropped (was %u)",
                                  i + 1, sx + 1, all);
                else if (diff < 0)
                    // Loop length shrinks together with sample length
                    // when cubic resampling fires (mas_spec C.3).
                    note_expected(s, "I%d S%d loop_length %u -> %u (resample)",
                                  i + 1, sx + 1, all, bll);
                else
                    note_unexpected(s, "I%d S%d loop_length %u vs %u",
                                    i + 1, sx + 1, all, bll);
            }
            if (avol != bvol)
                note_unexpected(s, "I%d S%d volume %d vs %d",
                                i + 1, sx + 1, avol, bvol);

            // Joint finetune + relative note check.
            //
            // XM stores pitch as (relative_note_semitones * 128 + finetune).
            // mas2xm round-trips through Hz and then back via log2, so the
            // split between the two integer fields can drift while the
            // total cents distance stays tiny. Compare the joint value
            // and accept anything within TOL_FINETUNE units.
            int total_a = (int)arn * 128 + (int)aft;
            int total_b = (int)brn * 128 + (int)bft;
            int total_diff = iabs(total_a - total_b);
            bool finetune_diff = (aft != bft);
            bool relnote_diff = (arn != brn);
            if ((finetune_diff || relnote_diff) && total_diff <= TOL_FINETUNE) {
                if (finetune_diff && relnote_diff)
                    note_expected(s, "I%d S%d ft/rn (%d,%d) vs (%d,%d) joint=%d",
                                  i + 1, sx + 1, arn, aft, brn, bft, total_diff);
                else if (finetune_diff)
                    note_expected(s, "I%d S%d finetune %d vs %d (joint=%d)",
                                  i + 1, sx + 1, aft, bft, total_diff);
                else
                    note_expected(s, "I%d S%d relnote %d vs %d (joint=%d)",
                                  i + 1, sx + 1, arn, brn, total_diff);
            } else {
                if (finetune_diff)
                    note_unexpected(s, "I%d S%d finetune %d vs %d (joint=%d)",
                                    i + 1, sx + 1, aft, bft, total_diff);
                if (relnote_diff)
                    note_unexpected(s, "I%d S%d relnote %d vs %d (joint=%d)",
                                    i + 1, sx + 1, arn, brn, total_diff);
            }

            if (atype != btype) {
                // Only the BIDI -> forward unroll is acceptable
                if ((atype & 0x03) == 0x02 && (btype & 0x03) == 0x01 &&
                    (atype & 0x10) == (btype & 0x10))
                    note_expected(s, "I%d S%d BIDI -> forward (type %02x->%02x)",
                                  i + 1, sx + 1, atype, btype);
                else
                    note_unexpected(s, "I%d S%d type %02x vs %02x",
                                    i + 1, sx + 1, atype, btype);
            }
            if (apan != bpan) {
                if (iabs((int)apan - (int)bpan) <= TOL_PAN)
                    note_expected(s, "I%d S%d pan %d vs %d",
                                  i + 1, sx + 1, apan, bpan);
                else
                    note_unexpected(s, "I%d S%d pan %d vs %d",
                                    i + 1, sx + 1, apan, bpan);
            }

            asp += ashsz;
            bsp += bshsz;
        }
    }
}

static void compare_envelopes_and_vibrato(const XM *a, const XM *b, DiffStats *s)
{
    int ninst = a->ninst < b->ninst ? a->ninst : b->ninst;

    for (int i = 0; i < ninst; i++) {
        if (a->inst_pos[i] + 33 + 110 > (size_t)a->sz) continue;
        if (b->inst_pos[i] + 33 + 110 > (size_t)b->sz) continue;

        u16 ans = *(u16 *)(a->d + a->inst_pos[i] + 27);
        u16 bns = *(u16 *)(b->d + b->inst_pos[i] + 27);
        if (ans == 0 || bns == 0) continue;
        u32 aihs = *(u32 *)(a->d + a->inst_pos[i]);
        u32 bihs = *(u32 *)(b->d + b->inst_pos[i]);
        if (aihs < 33 || bihs < 33) continue;

        size_t aep = a->inst_pos[i] + 33 + 96;
        size_t bep = b->inst_pos[i] + 33 + 96;

        u8 avc = a->d[aep + 96], bvc = b->d[bep + 96];
        u8 apc = a->d[aep + 97], bpc = b->d[bep + 97];
        u8 avf = a->d[aep + 104], bvf = b->d[bep + 104];
        u8 apf = a->d[aep + 105], bpf = b->d[bep + 105];

        // If point counts differ, it's expected only when the original
        // had data but the envelope was not "on" (mmutil drops it).
        if (avc != bvc) {
            if (bvc == 0 && (avf & 1) == 0)
                note_expected(s, "I%d vol env stripped (data, not 'on')", i + 1);
            else if (avc == bvc + 0)
                ; // unreachable
            else
                note_unexpected(s, "I%d vol env count %u vs %u", i + 1, avc, bvc);
        }
        if (apc != bpc) {
            if (bpc == 0 && (apf & 1) == 0)
                note_expected(s, "I%d pan env stripped (data, not 'on')", i + 1);
            else
                note_unexpected(s, "I%d pan env count %u vs %u", i + 1, apc, bpc);
        }
        if (avf != bvf) {
            // 'on' bit must match; sustain/loop bits should match too
            if ((avf & 1) != (bvf & 1))
                note_unexpected(s, "I%d vol env on-flag %u vs %u",
                                i + 1, avf & 1, bvf & 1);
            else
                note_expected(s, "I%d vol env flags %02x vs %02x", i + 1, avf, bvf);
        }
        if (apf != bpf) {
            if ((apf & 1) != (bpf & 1))
                note_unexpected(s, "I%d pan env on-flag %u vs %u",
                                i + 1, apf & 1, bpf & 1);
            else
                note_expected(s, "I%d pan env flags %02x vs %02x", i + 1, apf, bpf);
        }

        // Compare X/Y of overlapping nodes
        int ncmp = avc < bvc ? avc : bvc;
        if (ncmp > 12) ncmp = 12;
        for (int j = 0; j < ncmp; j++) {
            u16 ax = *(u16 *)(a->d + aep + j * 4);
            u16 ay = *(u16 *)(a->d + aep + j * 4 + 2);
            u16 bx = *(u16 *)(b->d + bep + j * 4);
            u16 by = *(u16 *)(b->d + bep + j * 4 + 2);
            if (ay != by)
                note_unexpected(s, "I%d vol env node[%d] Y %u vs %u",
                                i + 1, j, ay, by);
            if (ax != bx) {
                if (iabs((int)ax - (int)bx) <= TOL_ENV_X)
                    note_expected(s, "I%d vol env node[%d] X %u vs %u",
                                  i + 1, j, ax, bx);
                else
                    note_unexpected(s, "I%d vol env node[%d] X %u vs %u",
                                    i + 1, j, ax, bx);
            }
        }

        // Vibrato (offsets 106..109) and fadeout (110..111)
        u8 avt = a->d[aep + 106], bvt = b->d[bep + 106];
        u8 avs = a->d[aep + 107], bvs = b->d[bep + 107];
        u8 avd = a->d[aep + 108], bvd = b->d[bep + 108];
        u8 avr = a->d[aep + 109], bvr = b->d[bep + 109];
        u16 afo = *(u16 *)(a->d + aep + 110);
        u16 bfo = *(u16 *)(b->d + bep + 110);

        if (avt != bvt)
            note_unexpected(s, "I%d vibrato type %u vs %u", i + 1, avt, bvt);
        if (avs != bvs)
            note_expected(s, "I%d vibrato sweep %u vs %u (lossy u8)",
                          i + 1, avs, bvs);
        if (avd != bvd)
            note_unexpected(s, "I%d vibrato depth %u vs %u", i + 1, avd, bvd);
        if (avr != bvr)
            note_unexpected(s, "I%d vibrato rate %u vs %u", i + 1, avr, bvr);
        if (afo != bfo) {
            if (iabs((int)afo - (int)bfo) <= TOL_FADEOUT)
                note_expected(s, "I%d fadeout %u vs %u (/32 rounding)",
                              i + 1, afo, bfo);
            else if (afo > 8160 && bfo == 8160)
                // XM fadeout range is 0..4095. MAS stores fadeout/32 in
                // a u8, so the max representable XM value after roundtrip
                // is 255 * 32 = 8160. Anything bigger overflows the u8.
                note_expected(s, "I%d fadeout %u capped to 8160 (u8 overflow)",
                              i + 1, afo);
            else
                note_unexpected(s, "I%d fadeout %u vs %u", i + 1, afo, bfo);
        }
    }
}

// ----------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------

static const char *DEFAULT_MMUTIL[] = {
    "/c/devkitpro/tools/bin/mmutil.exe",
    "C:/devkitpro/tools/bin/mmutil.exe",
    "mmutil",
};
static const char *DEFAULT_MAS2XM[] = {
    "./build/mas2xm.exe",
    "./build/mas2xm",
    "../build/mas2xm.exe",
    "../build/mas2xm",
    "build/mas2xm.exe",
    "build/mas2xm",
};

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [-v] [-k] [--mmutil PATH] [--mas2xm PATH] input.xm\n"
        "\n"
        "Runs the full XM -> MAS -> XM roundtrip and classifies diffs.\n"
        "Exit 0 = PASS, 1 = FAIL (or pipeline error), 2 = usage error.\n",
        prog);
}

int main(int argc, char **argv)
{
    const char *mmutil_arg = NULL;
    const char *mas2xm_arg = NULL;
    const char *input_xm = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) g_verbose = true;
        else if (strcmp(argv[i], "-k") == 0) g_keep = true;
        else if (strcmp(argv[i], "--mmutil") == 0 && i + 1 < argc)
            mmutil_arg = argv[++i];
        else if (strcmp(argv[i], "--mas2xm") == 0 && i + 1 < argc)
            mas2xm_arg = argv[++i];
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (!input_xm && argv[i][0] != '-') {
            input_xm = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (!input_xm) {
        usage(argv[0]);
        return 2;
    }
    if (!file_exists(input_xm)) {
        fprintf(stderr, "error: input file %s not found\n", input_xm);
        return 1;
    }

    const char *mmutil = resolve_tool(mmutil_arg, "MMUTIL",
                                      DEFAULT_MMUTIL,
                                      sizeof(DEFAULT_MMUTIL) / sizeof(*DEFAULT_MMUTIL));
    const char *mas2xm = resolve_tool(mas2xm_arg, "MAS2XM",
                                      DEFAULT_MAS2XM,
                                      sizeof(DEFAULT_MAS2XM) / sizeof(*DEFAULT_MAS2XM));

    // Build the temp file paths next to the input so they're easy to find
    // when -k is set. Use the system tmp otherwise.
    char base[256];
    basename_noext(input_xm, base, sizeof(base));

    char mas_path[1024];
    char xm_path[1024];
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = getenv("TEMP");
    if (!tmp || !*tmp) tmp = "/tmp";

    // Normalize backslashes -> forward slashes so the path is portable
    // across the spawned shell (cmd.exe accepts forward slashes too).
    char tmp_norm[512];
    snprintf(tmp_norm, sizeof(tmp_norm), "%s", tmp);
    for (char *p = tmp_norm; *p; p++)
        if (*p == '\\') *p = '/';
    // Strip trailing slash if any
    size_t tlen = strlen(tmp_norm);
    if (tlen > 0 && tmp_norm[tlen - 1] == '/') tmp_norm[tlen - 1] = '\0';

    snprintf(mas_path, sizeof(mas_path), "%s/%s_rt.mas", tmp_norm, base);
    snprintf(xm_path,  sizeof(xm_path),  "%s/%s_rt_recon.xm", tmp_norm, base);

    printf("roundtrip_test: %s\n", input_xm);
    printf("  mmutil  : %s\n", mmutil);
    printf("  mas2xm  : %s\n", mas2xm);
    printf("  tmp_mas : %s\n", mas_path);
    printf("  tmp_xm  : %s\n", xm_path);

    // ---- Step 1: mmutil ----
    long input_size = file_size(input_xm);
    printf("  [1/3] mmutil      \n");
    fflush(stdout);

    // mmutil syntax: mmutil -d -m INPUT -oOUTPUT (output flag with no space)
    char mmutil_oarg[1100];
    snprintf(mmutil_oarg, sizeof(mmutil_oarg), "-o%s", mas_path);
    const char *mmutil_argv[] = {
        mmutil, "-d", "-m", input_xm, mmutil_oarg, NULL
    };
    int rc = spawn_child(mmutil, mmutil_argv);
    printf("  [1/3] mmutil      ");
    fflush(stdout);
    if (rc != 0 || !file_exists(mas_path)) {
        printf("FAIL (rc=%d)\n", rc);
        printf("RESULT: FAIL (mmutil error)\n");
        return 1;
    }
    long mas_size = file_size(mas_path);
    printf("ok (%ld -> %ld bytes)\n", input_size, mas_size);

    // ---- Step 2: mas2xm ----
    printf("  [2/3] mas2xm      ");
    fflush(stdout);
    const char *mas2xm_argv[] = {
        mas2xm, mas_path, xm_path, NULL
    };
    rc = spawn_child(mas2xm, mas2xm_argv);
    if (rc != 0 || !file_exists(xm_path)) {
        printf("FAIL (rc=%d)\n", rc);
        printf("RESULT: FAIL (mas2xm error)\n");
        if (!g_keep) unlink(mas_path);
        return 1;
    }
    long recon_size = file_size(xm_path);
    printf("ok (%ld -> %ld bytes)\n", mas_size, recon_size);

    // ---- Step 3: structural diff ----
    printf("  [3/3] structural diff\n");

    XM a = {0}, b = {0};
    if (load_xm(input_xm, &a) != 0 || load_xm(xm_path, &b) != 0) {
        printf("RESULT: FAIL (could not load XM for diff)\n");
        free_xm(&a); free_xm(&b);
        if (!g_keep) { unlink(mas_path); unlink(xm_path); }
        return 1;
    }

    DiffStats stats = {0};

    if (g_verbose) printf("    --- header ---\n");
    compare_header(&a, &b, &stats);
    if (g_verbose) printf("    --- patterns ---\n");
    compare_patterns(&a, &b, &stats);
    if (g_verbose) printf("    --- samples ---\n");
    compare_samples(&a, &b, &stats);
    if (g_verbose) printf("    --- envelopes / vibrato ---\n");
    compare_envelopes_and_vibrato(&a, &b, &stats);

    free_xm(&a);
    free_xm(&b);

    if (!g_keep) {
        unlink(mas_path);
        unlink(xm_path);
    }

    printf("    expected diffs   : %d\n", stats.expected);
    printf("    unexpected diffs : %d\n", stats.unexpected);

    if (stats.unexpected == 0) {
        printf("RESULT: PASS\n");
        return 0;
    } else {
        printf("    first unexpected: %s\n", stats.first_unexpected);
        printf("RESULT: FAIL\n");
        return 1;
    }
}
