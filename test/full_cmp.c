#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

typedef struct { u8 note,inst,vol,fx,param; } E;

typedef struct {
    u8 *d; long sz;
    u32 hdr_size;
    u16 song_len, restart, nch, npatt, ninst, flags, speed, tempo;
    u8 orders[256];
    size_t patt_pos[256];
    u16 patt_rows[256];
    size_t inst_pos[256];
} XM;

int load_xm(const char *file, XM *x) {
    FILE *f = fopen(file, "rb");
    if (!f) { printf("Cannot open %s\n", file); return 1; }
    fseek(f, 0, SEEK_END); x->sz = ftell(f); fseek(f, 0, SEEK_SET);
    x->d = malloc(x->sz); fread(x->d, 1, x->sz, f); fclose(f);

    x->hdr_size = *(u32*)(x->d+60);
    x->song_len = *(u16*)(x->d+64);
    x->restart  = *(u16*)(x->d+66);
    x->nch      = *(u16*)(x->d+68);
    x->npatt    = *(u16*)(x->d+70);
    x->ninst    = *(u16*)(x->d+72);
    x->flags    = *(u16*)(x->d+74);
    x->speed    = *(u16*)(x->d+76);
    x->tempo    = *(u16*)(x->d+78);
    memcpy(x->orders, x->d+80, 256);

    size_t pos = 60 + x->hdr_size;
    for (int p = 0; p < x->npatt; p++) {
        x->patt_pos[p] = pos;
        u32 phdr = *(u32*)(x->d+pos);
        x->patt_rows[p] = *(u16*)(x->d+pos+5);
        u16 dsize = *(u16*)(x->d+pos+7);
        pos += phdr + dsize;
    }
    for (int i = 0; i < x->ninst; i++) {
        x->inst_pos[i] = pos;
        u32 ihdr = *(u32*)(x->d+pos);
        u16 nsamp = *(u16*)(x->d+pos+27);
        size_t after = pos + ihdr;
        if (nsamp > 0 && ihdr >= 33) {
            u32 shsz = *(u32*)(x->d+pos+29);
            u32 total = 0;
            for (int s = 0; s < nsamp; s++)
                total += *(u32*)(x->d + after + s*shsz);
            pos = after + nsamp*shsz + total;
        } else {
            pos = after;
        }
    }
    return 0;
}

void decode_row(u8 *d, size_t *dp, int nch, E *row) {
    for (int c = 0; c < nch; c++) {
        row[c] = (E){0,0,0,0,0};
        u8 b = d[(*dp)++];
        if (b & 0x80) {
            if (b&1) row[c].note=d[(*dp)++];
            if (b&2) row[c].inst=d[(*dp)++];
            if (b&4) row[c].vol =d[(*dp)++];
            if (b&8) row[c].fx  =d[(*dp)++];
            if (b&16)row[c].param=d[(*dp)++];
        } else {
            row[c].note=b; row[c].inst=d[(*dp)++]; row[c].vol=d[(*dp)++];
            row[c].fx=d[(*dp)++]; row[c].param=d[(*dp)++];
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { printf("Usage: %s orig.xm recon.xm\n", argv[0]); return 1; }

    XM a, b;
    if (load_xm(argv[1], &a)) return 1;
    if (load_xm(argv[2], &b)) return 1;

    printf("=== HEADER ===\n");
    const char *names[] = {"song_len","restart","channels","patterns","instruments","flags","speed","tempo"};
    u16 av[] = {a.song_len,a.restart,a.nch,a.npatt,a.ninst,a.flags,a.speed,a.tempo};
    u16 bv[] = {b.song_len,b.restart,b.nch,b.npatt,b.ninst,b.flags,b.speed,b.tempo};
    for (int i = 0; i < 8; i++) {
        const char *tag = (av[i]==bv[i]) ? "  OK" : "DIFF";
        printf("  %-12s: %5u vs %5u  [%s]\n", names[i], av[i], bv[i], tag);
    }

    char ta[21]={0}, tb[21]={0};
    memcpy(ta, a.d+17, 20); memcpy(tb, b.d+17, 20);
    printf("  %-12s: \"%s\" vs \"%s\"  [%s]\n", "title", ta, tb,
           strcmp(ta,tb)==0 ? "  OK" : "DIFF (expected)");

    char ka[21]={0}, kb[21]={0};
    memcpy(ka, a.d+38, 20); memcpy(kb, b.d+38, 20);
    printf("  %-12s: \"%s\" vs \"%s\"  [%s]\n", "tracker", ka, kb,
           strcmp(ka,kb)==0 ? "  OK" : "DIFF (expected)");

    // Orders
    int ord_diff = 0;
    int cmp_len = a.song_len < b.song_len ? a.song_len : b.song_len;
    for (int i = 0; i < cmp_len; i++)
        if (a.orders[i] != b.orders[i]) ord_diff++;
    printf("  %-12s: %d diffs in %d entries  [%s]\n", "orders",
           ord_diff, cmp_len, ord_diff==0 ? "  OK" : "DIFF");

    // Patterns
    printf("\n=== PATTERNS ===\n");
    int min_ch = a.nch < b.nch ? a.nch : b.nch;
    int patt_diffs = 0, total_entry_diffs = 0;
    for (int p = 0; p < a.npatt && p < b.npatt; p++) {
        if (a.patt_rows[p] != b.patt_rows[p]) {
            printf("  Pattern %d: rows %d vs %d  [DIFF]\n", p, a.patt_rows[p], b.patt_rows[p]);
            patt_diffs++;
            continue;
        }
        u32 aph = *(u32*)(a.d+a.patt_pos[p]);
        u32 bph = *(u32*)(b.d+b.patt_pos[p]);
        size_t adp = a.patt_pos[p]+aph, bdp = b.patt_pos[p]+bph;

        int pdiff = 0;
        E ar[32], br[32];
        for (int r = 0; r < a.patt_rows[p]; r++) {
            decode_row(a.d, &adp, a.nch, ar);
            decode_row(b.d, &bdp, b.nch, br);
            for (int c = 0; c < min_ch; c++) {
                if (memcmp(&ar[c], &br[c], sizeof(E)) != 0) {
                    if (pdiff < 3)
                        printf("  P%d R%d C%d: [n%d i%d v%02x fx%d/%d] vs [n%d i%d v%02x fx%d/%d]\n",
                               p, r, c+1,
                               ar[c].note, ar[c].inst, ar[c].vol, ar[c].fx, ar[c].param,
                               br[c].note, br[c].inst, br[c].vol, br[c].fx, br[c].param);
                    pdiff++;
                }
            }
        }
        if (pdiff > 0) {
            if (pdiff > 3) printf("  ... +%d more diffs in pattern %d\n", pdiff-3, p);
            patt_diffs++;
            total_entry_diffs += pdiff;
        }
    }
    printf("  Summary: %d/%d patterns with diffs, %d total entry diffs\n",
           patt_diffs, a.npatt, total_entry_diffs);

    // Instruments/Samples
    printf("\n=== SAMPLES ===\n");
    int samp_diffs = 0;
    for (int i = 0; i < a.ninst && i < b.ninst; i++) {
        u16 ans = *(u16*)(a.d+a.inst_pos[i]+27);
        u16 bns = *(u16*)(b.d+b.inst_pos[i]+27);
        if (ans != bns) {
            printf("  Inst %d: nsamp %d vs %d\n", i+1, ans, bns);
            samp_diffs++;
            continue;
        }
        if (ans == 0) continue;
        u32 aihs = *(u32*)(a.d+a.inst_pos[i]);
        u32 bihs = *(u32*)(b.d+b.inst_pos[i]);
        size_t asp = a.inst_pos[i]+aihs, bsp = b.inst_pos[i]+bihs;
        u32 ashsz = *(u32*)(a.d+a.inst_pos[i]+29);
        u32 bshsz = *(u32*)(b.d+b.inst_pos[i]+29);

        for (int s = 0; s < ans; s++) {
            u32 alen=*(u32*)(a.d+asp), blen=*(u32*)(b.d+bsp);
            u32 als=*(u32*)(a.d+asp+4), bls=*(u32*)(b.d+bsp+4);
            u32 all=*(u32*)(a.d+asp+8), bll=*(u32*)(b.d+bsp+8);
            u8 avol=a.d[asp+12], bvol=b.d[bsp+12];
            int8_t aft=(int8_t)a.d[asp+13], bft=(int8_t)b.d[bsp+13];
            u8 atype=a.d[asp+14], btype=b.d[bsp+14];
            u8 apan=a.d[asp+15], bpan=b.d[bsp+15];
            int8_t arn=(int8_t)a.d[asp+16], brn=(int8_t)b.d[bsp+16];

            char buf[512]; buf[0]=0; int nd=0;
            if (alen!=blen) { nd++; sprintf(buf+strlen(buf)," len(%u/%u)",alen,blen); }
            if (als!=bls)   { nd++; sprintf(buf+strlen(buf)," ls(%u/%u)",als,bls); }
            if (all!=bll)   { nd++; sprintf(buf+strlen(buf)," ll(%u/%u)",all,bll); }
            if (avol!=bvol) { nd++; sprintf(buf+strlen(buf)," vol(%d/%d)",avol,bvol); }
            if (aft!=bft)   { nd++; sprintf(buf+strlen(buf)," ft(%d/%d)",aft,bft); }
            if (atype!=btype){nd++; sprintf(buf+strlen(buf)," type(%02x/%02x)",atype,btype); }
            if (apan!=bpan) { nd++; sprintf(buf+strlen(buf)," pan(%d/%d)",apan,bpan); }
            if (arn!=brn)   { nd++; sprintf(buf+strlen(buf)," rn(%d/%d)",arn,brn); }
            if (nd) { printf("  Inst %d samp %d:%s\n", i+1, s+1, buf); samp_diffs++; }

            asp += ashsz; bsp += bshsz;
        }
    }
    if (samp_diffs == 0) printf("  All sample headers match!\n");

    // Envelopes
    printf("\n=== ENVELOPES ===\n");
    int env_diffs = 0;
    for (int i = 0; i < a.ninst && i < b.ninst; i++) {
        u16 ans = *(u16*)(a.d+a.inst_pos[i]+27);
        if (ans == 0) continue;
        u32 aihs = *(u32*)(a.d+a.inst_pos[i]);
        u32 bihs = *(u32*)(b.d+b.inst_pos[i]);
        if (aihs < 33 || bihs < 33) continue;

        size_t aep = a.inst_pos[i]+33+96, bep = b.inst_pos[i]+33+96;

        // vol env: 48 bytes, pan env: 48 bytes, meta: 10 bytes
        int nd = 0;
        // vol env X/Y points
        for (int j = 0; j < 48; j++)
            if (a.d[aep+j] != b.d[bep+j]) nd++;
        // pan env X/Y points
        for (int j = 48; j < 96; j++)
            if (a.d[aep+j] != b.d[bep+j]) nd++;
        // counts, loops, flags
        for (int j = 96; j < 106; j++)
            if (a.d[aep+j] != b.d[bep+j]) nd++;
        if (nd) {
            printf("  Inst %d: %d envelope byte diffs\n", i+1, nd);

            // Show detail
            u8 avc = a.d[aep+96], bvc = b.d[bep+96];
            u8 apc = a.d[aep+97], bpc = b.d[bep+97];
            u8 avf = a.d[aep+104], bvf = b.d[bep+104];
            u8 apf = a.d[aep+105], bpf = b.d[bep+105];
            printf("    vol_npts: %d/%d  pan_npts: %d/%d  vol_flags: %02x/%02x  pan_flags: %02x/%02x\n",
                   avc, bvc, apc, bpc, avf, bvf, apf, bpf);
            // Show vol env points
            if (avc > 0 || bvc > 0) {
                int mx = avc > bvc ? avc : bvc;
                if (mx > 12) mx = 12;
                printf("    vol_env: ");
                for (int j = 0; j < mx; j++) {
                    u16 ax = *(u16*)(a.d+aep+j*4), ay = *(u16*)(a.d+aep+j*4+2);
                    u16 bx = *(u16*)(b.d+bep+j*4), by = *(u16*)(b.d+bep+j*4+2);
                    printf("(%u,%u)/(%u,%u) ", ax, ay, bx, by);
                }
                printf("\n");
            }
            env_diffs++;
        }
    }
    if (env_diffs == 0) printf("  All envelopes match!\n");

    // Vibrato
    printf("\n=== VIBRATO ===\n");
    int vib_diffs = 0;
    for (int i = 0; i < a.ninst && i < b.ninst; i++) {
        u16 ans = *(u16*)(a.d+a.inst_pos[i]+27);
        if (ans == 0) continue;
        size_t aep = a.inst_pos[i]+33+96;
        size_t bep = b.inst_pos[i]+33+96;
        // vibrato at offset 106..109
        u8 avt=a.d[aep+106], bvt=b.d[bep+106];
        u8 avs=a.d[aep+107], bvs=b.d[bep+107];
        u8 avd=a.d[aep+108], bvd=b.d[bep+108];
        u8 avr=a.d[aep+109], bvr=b.d[bep+109];
        u16 afo=*(u16*)(a.d+aep+110), bfo=*(u16*)(b.d+bep+110);
        if (avt!=bvt||avs!=bvs||avd!=bvd||avr!=bvr||afo!=bfo) {
            printf("  Inst %d: vib(%d,%d,%d,%d)/(%d,%d,%d,%d) fadeout(%d/%d)\n",
                   i+1, avt,avs,avd,avr, bvt,bvs,bvd,bvr, afo, bfo);
            vib_diffs++;
        }
    }
    if (vib_diffs == 0) printf("  All vibrato/fadeout match!\n");

    printf("\n=== SUMMARY ===\n");
    printf("  Pattern diffs:  %d\n", total_entry_diffs);
    printf("  Sample diffs:   %d\n", samp_diffs);
    printf("  Envelope diffs: %d\n", env_diffs);
    printf("  Vibrato diffs:  %d\n", vib_diffs);

    free(a.d); free(b.d);
    return 0;
}
