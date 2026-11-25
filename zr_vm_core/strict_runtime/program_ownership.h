#ifndef ZR_VM_STRICT_PROGRAM_OWNERSHIP_INTERNAL_H
#define ZR_VM_STRICT_PROGRAM_OWNERSHIP_INTERNAL_H

#include "zr_vm_strict/program.h"

static inline int zr_strict_is_scalar(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_I64 || kind == ZR_STRICT_VALUE_BOOL;
}

static inline int zr_strict_is_borrow(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_READONLY_BORROW;
}

/* Every incoming edge must agree on which owners and borrows are live. */
ZrStrictProgramStatus zr_strict_verify_ownership(const ZrStrictProgram *program, uint32_t *badPc);

#endif
