// SPDX-License-Identifier: MIT
// mas2xm - MAS effect -> XM effect reverse mapping
//
// This is the inverse of mmutil's CONV_XM_EFFECT (xm.c:280).
// MAS uses IT-style letter codes: A=1, B=2, ..., Z=26, plus extensions 27-30.
// XM uses numeric effect codes 0x00-0x23.

#include "effects.h"

// Shorthand: MAS effect byte for letter X = X - 'A' + 1
#define MAS_A  1
#define MAS_B  2
#define MAS_C  3
#define MAS_D  4
#define MAS_E  5
#define MAS_F  6
#define MAS_G  7
#define MAS_H  8
#define MAS_K  11
#define MAS_L  12
#define MAS_O  15
#define MAS_P  16
#define MAS_Q  17
#define MAS_R  18
#define MAS_S  19
#define MAS_T  20
#define MAS_V  22
#define MAS_W  23
#define MAS_X  24
#define MAS_J  10

// Extension effects
#define MAS_EXT_SETVOL  27
#define MAS_EXT_KEYOFF  28
#define MAS_EXT_ENVPOS  29
#define MAS_EXT_TREMOR  30

bool mas_to_xm_effect(u8 mas_fx, u8 mas_param, u8 *xm_fx, u8 *xm_param)
{
    *xm_fx = 0;
    *xm_param = 0;

    if (mas_fx == 0 && mas_param == 0)
        return true; // No effect

    switch (mas_fx) {
    case MAS_J: // Arpeggio -> XM 0xy
        *xm_fx = 0;
        *xm_param = mas_param;
        return true;

    case MAS_F: // Pitch slide up -> XM 1xx, E1y (fine), X1y (extra fine)
        if (mas_param >= 0xF0) {
            // Fine porta up -> XM E1y
            *xm_fx = 0x0E;
            *xm_param = 0x10 | (mas_param & 0x0F);
        } else if (mas_param >= 0xE0) {
            // Extra fine porta up -> XM X1y (effect 33)
            *xm_fx = 33;
            *xm_param = 0x10 | (mas_param & 0x0F);
        } else {
            // Normal porta up -> XM 1xx
            *xm_fx = 1;
            *xm_param = mas_param;
        }
        return true;

    case MAS_E: // Pitch slide down -> XM 2xx, E2y (fine), X2y (extra fine)
        if (mas_param >= 0xF0) {
            // Fine porta down -> XM E2y
            *xm_fx = 0x0E;
            *xm_param = 0x20 | (mas_param & 0x0F);
        } else if (mas_param >= 0xE0) {
            // Extra fine porta down -> XM X2y
            *xm_fx = 33;
            *xm_param = 0x20 | (mas_param & 0x0F);
        } else {
            // Normal porta down -> XM 2xx
            *xm_fx = 2;
            *xm_param = mas_param;
        }
        return true;

    case MAS_G: // Porta to note -> XM 3xx
        *xm_fx = 3;
        *xm_param = mas_param;
        return true;

    case MAS_H: // Vibrato -> XM 4xy
        *xm_fx = 4;
        *xm_param = mas_param;
        return true;

    case MAS_L: // Porta + volslide -> XM 5xy
        *xm_fx = 5;
        *xm_param = mas_param;
        return true;

    case MAS_K: // Vibrato + volslide -> XM 6xy
        *xm_fx = 6;
        *xm_param = mas_param;
        return true;

    case MAS_R: // Tremolo -> XM 7xy
        *xm_fx = 7;
        *xm_param = mas_param;
        return true;

    case MAS_X: // Set panning -> XM 8xx
        *xm_fx = 8;
        *xm_param = mas_param;
        return true;

    case MAS_O: // Sample offset -> XM 9xx
        *xm_fx = 9;
        *xm_param = mas_param;
        return true;

    case MAS_D: // Volume slide -> XM Axy
        *xm_fx = 0x0A;
        *xm_param = mas_param;
        return true;

    case MAS_B: // Position jump -> XM Bxx
        *xm_fx = 0x0B;
        *xm_param = mas_param;
        return true;

    case MAS_C: // Pattern break -> XM Dxx (hex to BCD)
        *xm_fx = 0x0D;
        // MAS stores hex, XM expects BCD
        *xm_param = (u8)(((mas_param / 10) << 4) | (mas_param % 10));
        return true;

    case MAS_A: // Set speed -> XM Fxx (param < 32)
        *xm_fx = 0x0F;
        *xm_param = mas_param;
        return true;

    case MAS_T: // Set tempo -> XM Fxx (param >= 32)
        *xm_fx = 0x0F;
        *xm_param = mas_param;
        return true;

    case MAS_V: // Set global volume -> XM Gxx (effect 16)
        *xm_fx = 16;
        *xm_param = mas_param;
        return true;

    case MAS_W: // Global volume slide -> XM Hxx (effect 17)
        *xm_fx = 17;
        *xm_param = mas_param;
        return true;

    case MAS_P: // Panning slide -> XM Pxx (effect 25)
        *xm_fx = 25;
        *xm_param = mas_param;
        return true;

    case MAS_Q: // Retrigger -> XM Rxx (effect 27)
        *xm_fx = 27;
        *xm_param = mas_param;
        return true;

    case MAS_S: // Extended effects -> XM Exy (effect 0x0E)
        switch (mas_param & 0xF0) {
        case 0x00: // Fine vol slide up -> XM EAy
            *xm_fx = 0x0E;
            *xm_param = 0xA0 | (mas_param & 0x0F);
            return true;
        case 0x10: // Fine vol slide down -> XM EBy
            *xm_fx = 0x0E;
            *xm_param = 0xB0 | (mas_param & 0x0F);
            return true;
        case 0x20: // Old retrigger -> XM E9y
            *xm_fx = 0x0E;
            *xm_param = 0x90 | (mas_param & 0x0F);
            return true;
        case 0x30: // Vibrato waveform -> XM E4y
            *xm_fx = 0x0E;
            *xm_param = 0x40 | (mas_param & 0x0F);
            return true;
        case 0x40: // Tremolo waveform -> XM E7y
            *xm_fx = 0x0E;
            *xm_param = 0x70 | (mas_param & 0x0F);
            return true;
        case 0xB0: // Pattern loop -> XM E6y
            *xm_fx = 0x0E;
            *xm_param = 0x60 | (mas_param & 0x0F);
            return true;
        case 0xC0: // Note cut -> XM ECy
            *xm_fx = 0x0E;
            *xm_param = 0xC0 | (mas_param & 0x0F);
            return true;
        case 0xD0: // Note delay -> XM EDy
            *xm_fx = 0x0E;
            *xm_param = 0xD0 | (mas_param & 0x0F);
            return true;
        case 0xE0: // Pattern delay -> XM EEy
            *xm_fx = 0x0E;
            *xm_param = 0xE0 | (mas_param & 0x0F);
            return true;
        case 0xF0: // Special event -> XM EFy
            *xm_fx = 0x0E;
            *xm_param = 0xF0 | (mas_param & 0x0F);
            return true;
        default:
            // Unmapped S-effect sub-command
            return false;
        }

    // Extension effects
    case MAS_EXT_SETVOL: // Set volume (compat) -> XM Cxx (effect 0x0C)
        *xm_fx = 0x0C;
        *xm_param = mas_param;
        return true;

    case MAS_EXT_KEYOFF: // Key off -> XM Kxx (effect 20)
        *xm_fx = 20;
        *xm_param = mas_param;
        return true;

    case MAS_EXT_ENVPOS: // Set envelope position -> XM Lxx (effect 21)
        *xm_fx = 21;
        *xm_param = mas_param;
        return true;

    case MAS_EXT_TREMOR: // Tremor -> XM Txx (effect 29)
        *xm_fx = 29;
        *xm_param = mas_param;
        return true;

    default:
        // Unknown or unsupported MAS effect
        return false;
    }
}
