#ifndef ZR_VM_PARSER_COMPILER_QUICKENING_NULL_STATE_H
#define ZR_VM_PARSER_COMPILER_QUICKENING_NULL_STATE_H

#include "zr_vm_core/function.h"

/* Prove the old value is plain NULL with this clear removed from the CFG. */
TZrBool compiler_quickening_slot_is_null_without_clear(
        const SZrFunction *function, TZrUInt32 instructionIndex, TZrUInt32 stackSlot);

#endif
