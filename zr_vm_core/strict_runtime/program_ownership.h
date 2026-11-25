#ifndef ZR_VM_STRICT_PROGRAM_OWNERSHIP_INTERNAL_H
#define ZR_VM_STRICT_PROGRAM_OWNERSHIP_INTERNAL_H

#include "zr_vm_strict/program.h"

static inline int zr_strict_is_scalar(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_I64 || kind == ZR_STRICT_VALUE_BOOL;
}

static inline int zr_strict_is_record_borrow(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_READONLY_BORROW;
}

static inline int zr_strict_is_byte_view(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BYTE_VIEW || kind == ZR_STRICT_VALUE_READONLY_BYTE_VIEW;
}

static inline int zr_strict_is_borrow(uint8_t kind) {
    return zr_strict_is_record_borrow(kind) || zr_strict_is_byte_view(kind);
}

static inline int zr_strict_borrow_is_writable(uint8_t kind) {
    return kind == ZR_STRICT_VALUE_BORROW || kind == ZR_STRICT_VALUE_BYTE_VIEW;
}

static inline int zr_strict_uses_frame(const ZrStrictProgram *program) {
    if (program->layoutCount) return 1;
    for (uint32_t slot = 0; slot < program->slotCount; ++slot)
        if (!zr_strict_is_scalar(program->kinds[slot])) return 1;
    return 0;
}

/* Every incoming edge must agree on which owners and borrows are live. */
ZrStrictProgramStatus zr_strict_verify_ownership(const ZrStrictProgram *program, uint32_t *badPc, int liveReceiver);
ZrStrictProgramStatus zr_strict_run_frame(ZrStrictFrame *frame, ZrStrictResult *result, uint32_t *badPc);

static inline ZrStrictProgram zr_strict_destructor_program(const ZrStrictProgram *owner, const ZrStrictDestructor *body) {
    ZrStrictProgram program = {.kinds = body->kinds, .slotCount = body->slotCount, .instructions = body->instructions,
            .instructionCount = body->instructionCount, .slotLayouts = body->slotLayouts,
            .layouts = owner->layouts, .layoutCount = owner->layoutCount};
    return program;
}

#endif
