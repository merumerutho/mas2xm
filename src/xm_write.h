// SPDX-License-Identifier: MIT
// mas2xm - XM file writer

#ifndef MAS2XM_XM_WRITE_H
#define MAS2XM_XM_WRITE_H

#include "types.h"

// Write a Module to an XM file.
// Returns 0 on success, non-zero on error.
int xm_write(const Module *mod, const char *filename, bool verbose);

#endif // MAS2XM_XM_WRITE_H
