#ifndef ZR_VM_FUNCTION_BYTECODE_INTERNAL_H
#define ZR_VM_FUNCTION_BYTECODE_INTERNAL_H
#include "zr_vm_core/function.h"
/* Operand/control-flow validation for legacy nonempty host bodies. These may
 * fall through the end and must retain PC and slot guards during execution. */
TZrBool function_verify_guarded_instructions(const SZrFunction *function);
#endif
