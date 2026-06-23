#ifndef ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H
#define ZR_VM_PARSER_BACKEND_AOT_C_SCALAR_LOCALS_H

#include <stdio.h>

#include "backend_aot_exec_ir.h"

void backend_aot_write_c_scalar_locals(FILE *file, const SZrAotExecIrFunction *functionIr);
TZrBool backend_aot_c_scalar_locals_has_bool_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_f64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_i64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_has_u64_slot(const SZrAotExecIrFunction *functionIr, TZrUInt32 slot);
TZrBool backend_aot_c_scalar_locals_bool_written_before(const SZrAotExecIrFunction *functionIr,
                                                        TZrUInt32 slot,
                                                        TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_f64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_i64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_u64_written_before(const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 slot,
                                                       TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_direct_return_i64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_direct_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                 TZrUInt32 slot,
                                                                 TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_direct_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_direct_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_infer_return_bool_local(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 slot,
                                                                TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_infer_return_u64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_can_infer_return_f64_local(const SZrAotExecIrFunction *functionIr,
                                                               TZrUInt32 slot,
                                                               TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_reset_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                              TZrUInt32 slot,
                                                              TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_reset2_can_skip_value_slots(const SZrAotExecIrFunction *functionIr,
                                                                TZrUInt32 firstSlot,
                                                                TZrUInt32 secondSlot,
                                                                TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_i64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_f64_constant_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                     TZrUInt32 slot,
                                                                     TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_f64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                    TZrUInt32 slot,
                                                                    TZrUInt32 execInstructionIndex);
TZrBool backend_aot_c_scalar_locals_u64_result_can_skip_value_slot(const SZrAotExecIrFunction *functionIr,
                                                                   TZrUInt32 slot,
                                                                   TZrUInt32 execInstructionIndex);

#endif
