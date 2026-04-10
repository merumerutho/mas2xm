// SPDX-License-Identifier: MIT
// mas2xm - MAS binary reader

#ifndef MAS2XM_MAS_READ_H
#define MAS2XM_MAS_READ_H

#include "types.h"

// Read a .mas file from a memory buffer into a Module struct.
// Returns 0 on success, non-zero on error.
// On success, the caller must call module_free() when done.
int mas_read(const u8 *data, size_t size, Module *mod, bool verbose);

#endif // MAS2XM_MAS_READ_H
