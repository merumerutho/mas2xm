// SPDX-License-Identifier: MIT
// mas2xm - MAS effect -> XM effect reverse mapping

#ifndef MAS2XM_EFFECTS_H
#define MAS2XM_EFFECTS_H

#include "types.h"

// Convert a MAS effect+param pair back to XM effect+param.
// Sets *xm_fx and *xm_param. Returns false if the effect has no XM equivalent.
bool mas_to_xm_effect(u8 mas_fx, u8 mas_param, u8 *xm_fx, u8 *xm_param);

#endif // MAS2XM_EFFECTS_H
