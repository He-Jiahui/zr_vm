#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

void backend_aot_write_c_scalar_locals(FILE *file, const SZrAotExecIrFunction *functionIr);
TZrBool backend_aot_c_scalar_locals_has_bool_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_f64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_i64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_u64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_i64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);

#endif
