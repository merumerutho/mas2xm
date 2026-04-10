#!/bin/bash
# Batch roundtrip test: XM -> MAS (mmutil) -> XM (mas2xm) -> compare (full_cmp)
#
# Usage: batch_test.sh [<xm_source> ...]
#
# Each argument may be a directory (scanned recursively for *.xm) or an
# individual .xm file. With no arguments the script scans $SONGS_DIR,
# which defaults to test/songs/ — drop your own .xm fixtures there and
# rerun. Paths to the external tools and the log file can be overridden
# via MMUTIL, MAS2XM, FULL_CMP, and LOG environment variables.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SONGS_DIR="${SONGS_DIR:-$SCRIPT_DIR/songs}"
MMUTIL="${MMUTIL:-/c/devkitpro/tools/bin/mmutil.exe}"
MAS2XM="${MAS2XM:-$REPO_ROOT/build/mas2xm.exe}"
FULL_CMP="${FULL_CMP:-$REPO_ROOT/build/test/full_cmp.exe}"
LOG="${LOG:-$SCRIPT_DIR/batch_results.log}"

if [ $# -lt 1 ]; then
    set -- "$SONGS_DIR"
fi

for src in "$@"; do
    if [ ! -e "$src" ]; then
        echo "error: input not found: $src" >&2
        if [ "$src" = "$SONGS_DIR" ]; then
            echo "hint: drop .xm files into $SONGS_DIR or pass a path explicitly" >&2
        fi
        exit 2
    fi
done

for tool in "$MMUTIL" "$MAS2XM" "$FULL_CMP"; do
    if [ ! -x "$tool" ] && [ ! -f "$tool" ]; then
        echo "error: required tool not found: $tool" >&2
        exit 2
    fi
done

TMPDIR=$(mktemp -d)

trap "rm -rf $TMPDIR" EXIT

# Counters
total=0
mmutil_fail=0
mas2xm_fail=0
patt_diff_files=0
clean_pass=0

echo "Batch roundtrip test - $(date)" | tee "$LOG"
echo "Temp dir: $TMPDIR" | tee -a "$LOG"
echo "========================================" | tee -a "$LOG"

while IFS= read -r xm_file; do
    total=$((total + 1))
    basename=$(basename "$xm_file")
    safe_name=$(echo "$basename" | tr ' ' '_')
    idx=$total

    mas_file="$TMPDIR/${idx}_${safe_name%.xm}.mas"
    recon_file="$TMPDIR/${idx}_${safe_name%.xm}_recon.xm"

    echo "" | tee -a "$LOG"
    echo "--- [$idx] $xm_file ---" | tee -a "$LOG"

    # Step 1: XM -> MAS via mmutil
    mmutil_out=$("$MMUTIL" -d -m "$xm_file" -o"$mas_file" 2>&1)
    mmutil_rc=$?
    if [ $mmutil_rc -ne 0 ] || [ ! -f "$mas_file" ]; then
        echo "  MMUTIL FAILED (rc=$mmutil_rc)" | tee -a "$LOG"
        echo "  $mmutil_out" >> "$LOG"
        mmutil_fail=$((mmutil_fail + 1))
        continue
    fi

    # Step 2: MAS -> XM via mas2xm
    mas2xm_out=$("$MAS2XM" "$mas_file" "$recon_file" 2>&1)
    mas2xm_rc=$?
    if [ $mas2xm_rc -ne 0 ] || [ ! -f "$recon_file" ]; then
        echo "  MAS2XM FAILED (rc=$mas2xm_rc)" | tee -a "$LOG"
        echo "  $mas2xm_out" >> "$LOG"
        mas2xm_fail=$((mas2xm_fail + 1))
        continue
    fi

    # Step 3: Compare original vs reconstructed
    cmp_out=$("$FULL_CMP" "$xm_file" "$recon_file" 2>&1)
    cmp_rc=$?

    echo "$cmp_out" >> "$LOG"

    # Extract pattern diffs from summary
    patt_diffs=$(echo "$cmp_out" | grep "Pattern diffs:" | awk '{print $NF}')
    samp_diffs=$(echo "$cmp_out" | grep "Sample diffs:" | awk '{print $NF}')
    env_diffs=$(echo "$cmp_out" | grep "Envelope diffs:" | awk '{print $NF}')
    vib_diffs=$(echo "$cmp_out" | grep "Vibrato diffs:" | awk '{print $NF}')

    if [ "$patt_diffs" = "0" ] && [ "$samp_diffs" = "0" ] && [ "$env_diffs" = "0" ] && [ "$vib_diffs" = "0" ]; then
        echo "  CLEAN PASS" | tee -a "$LOG"
        clean_pass=$((clean_pass + 1))
    else
        echo "  DIFFS: patt=$patt_diffs samp=$samp_diffs env=$env_diffs vib=$vib_diffs" | tee -a "$LOG"
        if [ "$patt_diffs" != "0" ]; then
            patt_diff_files=$((patt_diff_files + 1))
        fi
    fi

done < <(
    for src in "$@"; do
        if [ -d "$src" ]; then
            find "$src" -iname '*.xm' -type f
        else
            printf '%s\n' "$src"
        fi
    done | sort
)

echo "" | tee -a "$LOG"
echo "========================================" | tee -a "$LOG"
echo "         BATCH TEST SUMMARY" | tee -a "$LOG"
echo "========================================" | tee -a "$LOG"
echo "  Total XM files:         $total" | tee -a "$LOG"
echo "  mmutil failures:        $mmutil_fail" | tee -a "$LOG"
echo "  mas2xm failures:        $mas2xm_fail" | tee -a "$LOG"
echo "  Files with pattern diffs: $patt_diff_files" | tee -a "$LOG"
echo "  Clean pass (0 diffs):   $clean_pass" | tee -a "$LOG"
echo "  Other diffs (no patt):  $((total - mmutil_fail - mas2xm_fail - patt_diff_files - clean_pass))" | tee -a "$LOG"
echo "========================================" | tee -a "$LOG"
echo "Full log: $LOG"
