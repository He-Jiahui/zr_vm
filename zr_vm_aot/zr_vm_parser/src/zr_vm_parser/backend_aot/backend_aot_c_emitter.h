#ifndef ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H
#define ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

typedef struct SZrAotExecIrFrameLayout SZrAotExecIrFrameLayout;
typedef struct SZrAotExecIrFunction SZrAotExecIrFunction;

const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_requires_materialization(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex);
TZrBool backend_aot_c_can_emit_typed_i64_no_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_i64_one_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_i64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_i64_three_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_no_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_one_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_three_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_no_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_one_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_three_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_no_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_one_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_two_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_three_arg_thunk(const SZrFunction *function);
TZrBool backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(const SZrFunction *function);
void backend_aot_write_c_direct_primitive_constant(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 execInstructionIndex,
                                                   const SZrTypeValue *constantValue);
void backend_aot_write_c_direct_set_constant(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 constantIndex);
void backend_aot_write_c_direct_constant_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex);
void backend_aot_write_c_direct_callable_constant(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 constantIndex,
                                                  TZrUInt32 callableFlatIndex);
void backend_aot_write_c_direct_get_sub_function(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 childFunctionIndex,
                                                  TZrUInt32 callableFlatIndex);
void backend_aot_write_c_create_closure(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex);
void backend_aot_write_c_unsupported_callable_constant_materialization(FILE *file,
                                                                       TZrUInt32 destinationSlot,
                                                                       TZrUInt32 constantIndex);
void backend_aot_write_c_unsupported_create_closure_materialization(FILE *file,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 constantIndex,
                                                                    TZrUInt32 captureCount);
void backend_aot_write_c_unsupported_get_sub_function_materialization(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 childFunctionIndex,
                                                                      TZrUInt32 captureCount);
void backend_aot_write_c_direct_stack_copy(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot,
                                           TZrBool skipScalarLocalSync);
void backend_aot_write_c_direct_reset_stack_null(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_reset_stack_null_scalar_local_skip(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_direct_reset_stack_null2(FILE *file, TZrUInt32 firstSlot, TZrUInt32 secondSlot);
void backend_aot_write_c_reset_stack_null2_scalar_local_skip(FILE *file,
                                                             TZrUInt32 firstSlot,
                                                             TZrUInt32 secondSlot);
void backend_aot_write_c_get_closure_value(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 closureIndex);
void backend_aot_write_c_set_closure_value(FILE *file, TZrUInt32 sourceSlot, TZrUInt32 closureIndex);
void backend_aot_write_c_direct_add_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot);
void backend_aot_write_c_direct_add_int_const(FILE *file,
                                              const SZrFunction *function,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_add_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_add_signed_const(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_add_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex);
void backend_aot_write_c_direct_sub_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mul_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex);
void backend_aot_write_c_direct_div_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mod_signed_load_const(FILE *file,
                                                      const SZrFunction *function,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 materializedConstantSlot,
                                                      TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add_signed_load_stack(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mul_signed_load_stack(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);
void backend_aot_write_c_direct_add_signed_load_stack_load_const(FILE *file,
                                                                 const SZrFunction *function,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 sourceSlot,
                                                                 TZrUInt32 materializedStackSlot,
                                                                 TZrUInt32 materializedConstantSlot,
                                                                 TZrUInt32 constantIndex);
void backend_aot_write_c_direct_sub_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mul_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_div_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mod_signed_load_stack_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 sourceSlot,
                                                            TZrUInt32 materializedStackSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_sub_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_signed_const(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_sub_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex);
void backend_aot_write_c_direct_add_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_get_global(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_direct_create_object(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_direct_create_array(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_direct_typeof(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_object(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex);
void backend_aot_write_c_direct_to_struct(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 typeNameConstantIndex);
void backend_aot_write_c_gc_safepoint(FILE *file, const char *indent, const char *marker);
void backend_aot_write_c_direct_jump(FILE *file,
                                     TZrUInt32 functionIndex,
                                     TZrUInt32 targetInstructionIndex,
                                     TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 execInstructionIndex,
                                        TZrUInt32 targetInstructionIndex,
                                        TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if_bool_false(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 functionIndex,
                                                   TZrUInt32 conditionSlot,
                                                   TZrUInt32 execInstructionIndex,
                                                   TZrUInt32 targetInstructionIndex,
                                                   TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if_greater_signed(FILE *file,
                                                       const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 functionIndex,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 execInstructionIndex,
                                                       TZrUInt32 targetInstructionIndex,
                                                       TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if_less_equal_signed(FILE *file,
                                                          const SZrAotExecIrFunction *functionIr,
                                                          TZrUInt32 functionIndex,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot,
                                                          TZrUInt32 execInstructionIndex,
                                                          TZrUInt32 targetInstructionIndex,
                                                          TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if_not_equal_signed(FILE *file,
                                                         const SZrAotExecIrFunction *functionIr,
                                                         TZrUInt32 functionIndex,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         TZrUInt32 execInstructionIndex,
                                                         TZrUInt32 targetInstructionIndex,
                                                         TZrBool isBackEdge);
void backend_aot_write_c_direct_jump_if_not_equal_signed_const(FILE *file,
                                                               const SZrAotExecIrFunction *functionIr,
                                                               const SZrFunction *function,
                                                               TZrUInt32 functionIndex,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 constantIndex,
                                                               TZrUInt32 execInstructionIndex,
                                                               TZrUInt32 targetInstructionIndex,
                                                               TZrBool isBackEdge);
void backend_aot_write_c_direct_to_bool(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot,
                                        TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_to_int(FILE *file,
                                       const SZrAotExecIrFunction *functionIr,
                                       TZrUInt32 destinationSlot,
                                       TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint(FILE *file,
                                        const SZrAotExecIrFunction *functionIr,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float(FILE *file,
                                         const SZrAotExecIrFunction *functionIr,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_publish_exports(FILE *file);
void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_return_i64_local(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_return_bool_local(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_return_u64_local(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_return_f64_local(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports);
void backend_aot_write_c_begin_instruction(FILE *file, TZrUInt32 instructionIndex, TZrUInt32 stepFlags);
void backend_aot_write_c_direct_mark_to_be_closed(FILE *file, TZrUInt32 slotIndex);
void backend_aot_write_c_direct_close_scope(FILE *file, TZrUInt32 cleanupCount);
void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_return_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_weak(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_upgrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_release(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_mul_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_mul_signed_const(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_mul_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mul_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mul_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mul(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_int(FILE *file,
                                        TZrUInt32 destinationSlot,
                                        TZrUInt32 leftSlot,
                                        TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_int_const(FILE *file,
                                              const SZrFunction *function,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 constantIndex);
void backend_aot_write_c_direct_bitwise_not(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_bitwise_and(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_bitwise_or(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_bitwise_xor(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_shift_left_int(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 leftSlot,
                                               TZrUInt32 rightSlot);
void backend_aot_write_c_direct_shift_right_int(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 leftSlot,
                                                TZrUInt32 rightSlot);
void backend_aot_write_c_direct_shift_left(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_shift_right(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_bitwise_shift_left(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);
void backend_aot_write_c_direct_bitwise_shift_right(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_neg(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_neg_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 sourceSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_neg_float(FILE *file,
                                          const SZrAotExecIrFunction *functionIr,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 sourceSlot,
                                          TZrUInt32 execInstructionIndex);
void backend_aot_write_c_unsupported_meta_value_access(FILE *file,
                                                       const char *opcodeName,
                                                       TZrUInt32 primarySlot,
                                                       TZrUInt32 secondarySlot,
                                                       TZrUInt32 memberOrCacheIndex);
void backend_aot_write_c_unsupported_dynamic_value_access(FILE *file,
                                                          const char *opcodeName,
                                                          TZrUInt32 primarySlot,
                                                          TZrUInt32 secondarySlot,
                                                          TZrUInt32 operandIndex);
void backend_aot_write_c_direct_get_member(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 memberId,
                                           TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_member(FILE *file,
                                           TZrUInt32 sourceSlot,
                                           TZrUInt32 receiverSlot,
                                           TZrUInt32 memberId,
                                           TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_member_new_owner_no_write_barrier(FILE *file,
                                                                      TZrUInt32 sourceSlot,
                                                                      TZrUInt32 receiverSlot,
                                                                      TZrUInt32 memberId,
                                                                      TZrUInt32 deoptId);
void backend_aot_write_c_direct_get_member_slot(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex,
                                                TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_member_slot(FILE *file,
                                                TZrUInt32 sourceSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex,
                                                TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_member_slot_new_owner_no_write_barrier(FILE *file,
                                                                           TZrUInt32 sourceSlot,
                                                                           TZrUInt32 receiverSlot,
                                                                           TZrUInt32 cacheIndex,
                                                                           TZrUInt32 deoptId);
void backend_aot_write_c_direct_get_by_index(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 keySlot,
                                             TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_by_index(FILE *file,
                                             TZrUInt32 sourceSlot,
                                             TZrUInt32 receiverSlot,
                                             TZrUInt32 keySlot,
                                             TZrUInt32 deoptId);
void backend_aot_write_c_direct_set_by_index_new_owner_no_write_barrier(FILE *file,
                                                                        TZrUInt32 sourceSlot,
                                                                        TZrUInt32 receiverSlot,
                                                                        TZrUInt32 keySlot,
                                                                        TZrUInt32 deoptId);
void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot,
                                              TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_not_equal(FILE *file,
                                                  const SZrAotExecIrFunction *functionIr,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot,
                                                  TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_equal_string(FILE *file,
                                                     const SZrAotExecIrFunction *functionIr,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_not_equal_string(FILE *file,
                                                         const SZrAotExecIrFunction *functionIr,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_and(FILE *file,
                                            const SZrAotExecIrFunction *functionIr,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot,
                                            TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_or(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_equal_bool(FILE *file,
                                                   const SZrAotExecIrFunction *functionIr,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot,
                                                   TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_not_equal_bool(FILE *file,
                                                       const SZrAotExecIrFunction *functionIr,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_not(FILE *file,
                                            const SZrAotExecIrFunction *functionIr,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_not_bool(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 sourceSlot,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_logical_equal_signed(FILE *file,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal_signed(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_equal_unsigned(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal_unsigned(FILE *file,
                                                           TZrUInt32 destinationSlot,
                                                           TZrUInt32 leftSlot,
                                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_equal_float(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal_float(FILE *file,
                                                        TZrUInt32 destinationSlot,
                                                        TZrUInt32 leftSlot,
                                                        TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_signed(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_unsigned(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_float(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 leftSlot,
                                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_signed(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_unsigned(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_float(FILE *file,
                                                      TZrUInt32 destinationSlot,
                                                      TZrUInt32 leftSlot,
                                                      TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_equal_signed(FILE *file,
                                                          TZrUInt32 destinationSlot,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_equal_unsigned(FILE *file,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_less_equal_float(FILE *file,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_equal_signed(FILE *file,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 leftSlot,
                                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_equal_unsigned(FILE *file,
                                                               TZrUInt32 destinationSlot,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_greater_equal_float(FILE *file,
                                                            TZrUInt32 destinationSlot,
                                                            TZrUInt32 leftSlot,
                                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_mod_signed_const(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_add_signed_mod_const(FILE *file,
                                                     const SZrFunction *function,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot,
                                                     TZrUInt32 constantIndex);
void backend_aot_write_c_direct_mod_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex);
void backend_aot_write_c_direct_div(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_div_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_div_signed_const(FILE *file,
                                                 const SZrAotExecIrFunction *functionIr,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex,
                                                 TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_div_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_div_unsigned_const(FILE *file,
                                                   const SZrFunction *function,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 constantIndex);
void backend_aot_write_c_direct_div_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_pow(FILE *file,
                                    const SZrAotExecIrFunction *functionIr,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_pow_signed(FILE *file,
                                           const SZrAotExecIrFunction *functionIr,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot,
                                           TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_pow_unsigned(FILE *file,
                                             const SZrAotExecIrFunction *functionIr,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot,
                                             TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_pow_float(FILE *file,
                                          const SZrAotExecIrFunction *functionIr,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot,
                                          TZrUInt32 execInstructionIndex);
void backend_aot_write_c_direct_super_array_get_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot);
void backend_aot_write_c_direct_super_array_set_int(FILE *file,
                                                    TZrUInt32 sourceSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot);
void backend_aot_write_c_direct_super_array_set_int_new_owner_no_write_barrier(FILE *file,
                                                                               TZrUInt32 sourceSlot,
                                                                               TZrUInt32 receiverSlot,
                                                                               TZrUInt32 keySlot);
void backend_aot_write_c_direct_super_array_add_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_super_array_add_int4(FILE *file,
                                                     TZrUInt32 receiverBaseSlot,
                                                     TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_super_array_add_int4_const(FILE *file,
                                                           const SZrFunction *function,
                                                           TZrUInt32 receiverBaseSlot,
                                                           TZrUInt32 constantIndex);
void backend_aot_write_c_direct_super_array_fill_int4_const(FILE *file,
                                                            const SZrFunction *function,
                                                            TZrUInt32 receiverBaseSlot,
                                                            TZrUInt32 countSlot,
                                                            TZrUInt32 constantIndex);
void backend_aot_write_c_direct_iter_init(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iterableSlot);
void backend_aot_write_c_direct_iter_move_next(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot);
void backend_aot_write_c_direct_iter_current(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 iteratorSlot);
void backend_aot_write_c_direct_iter_move_next_jump_if_false(FILE *file,
                                                             TZrUInt32 functionIndex,
                                                             TZrUInt32 destinationSlot,
                                                             TZrUInt32 iteratorSlot,
                                                             TZrUInt32 targetInstructionIndex,
                                                             TZrBool isBackEdge);
void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_unsupported_meta_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 receiverSlot,
                                               TZrUInt32 argumentCount);
void backend_aot_write_c_unsupported_instruction(FILE *file,
                                                 TZrUInt32 functionFlatIndex,
                                                 TZrUInt32 instructionIndex,
                                                 TZrUInt32 opcode);
void backend_aot_write_c_unsupported_instruction_expr(FILE *file,
                                                      TZrUInt32 functionFlatIndex,
                                                      const char *instructionIndexExpression,
                                                      const char *opcodeExpression);
void backend_aot_write_c_static_direct_function_call(FILE *file,
                                                     const SZrAotExecIrFunction *functionIr,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 execInstructionIndex,
                                                     TZrUInt32 calleeFlatIndex);
void backend_aot_write_c_static_direct_i64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_i64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_i64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk);
void backend_aot_write_c_static_direct_i64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk);
void backend_aot_write_c_static_direct_bool_no_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_bool_one_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 argumentSlot,
                                                                  TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_bool_two_arg_function_call(FILE *file,
                                                                  TZrUInt32 destinationSlot,
                                                                  TZrUInt32 functionSlot,
                                                                  TZrUInt32 calleeFlatIndex,
                                                                  TZrUInt32 firstArgumentSlot,
                                                                  TZrUInt32 secondArgumentSlot,
                                                                  TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_bool_three_arg_function_call(FILE *file,
                                                                    TZrUInt32 destinationSlot,
                                                                    TZrUInt32 functionSlot,
                                                                    TZrUInt32 calleeFlatIndex,
                                                                    TZrUInt32 firstArgumentSlot,
                                                                    TZrUInt32 secondArgumentSlot,
                                                                    TZrUInt32 thirdArgumentSlot,
                                                                    TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_i64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_u64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_f64_bool_two_arg_function_call(FILE *file,
                                                                      TZrUInt32 destinationSlot,
                                                                      TZrUInt32 functionSlot,
                                                                      TZrUInt32 calleeFlatIndex,
                                                                      TZrUInt32 firstArgumentSlot,
                                                                      TZrUInt32 secondArgumentSlot,
                                                                      TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_u64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_u64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_u64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk);
void backend_aot_write_c_static_direct_u64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk);
void backend_aot_write_c_static_direct_f64_no_arg_function_call(FILE *file,
                                                                TZrUInt32 destinationSlot,
                                                                TZrUInt32 functionSlot,
                                                                TZrUInt32 calleeFlatIndex,
                                                                TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_f64_one_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 argumentSlot,
                                                                 TZrBool syncStackSlot);
void backend_aot_write_c_static_direct_f64_two_arg_function_call(FILE *file,
                                                                 TZrUInt32 destinationSlot,
                                                                 TZrUInt32 functionSlot,
                                                                 TZrUInt32 calleeFlatIndex,
                                                                 TZrUInt32 firstArgumentSlot,
                                                                 TZrUInt32 secondArgumentSlot,
                                                                 TZrBool syncStackSlot,
                                                                 TZrBool passStateToThunk);
void backend_aot_write_c_static_direct_f64_three_arg_function_call(FILE *file,
                                                                   TZrUInt32 destinationSlot,
                                                                   TZrUInt32 functionSlot,
                                                                   TZrUInt32 calleeFlatIndex,
                                                                   TZrUInt32 firstArgumentSlot,
                                                                   TZrUInt32 secondArgumentSlot,
                                                                   TZrUInt32 thirdArgumentSlot,
                                                                   TZrBool syncStackSlot,
                                                                   TZrBool passStateToThunk);
void backend_aot_write_c_direct_function_call(FILE *file,
                                              const SZrAotExecIrFunction *functionIr,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount);
void backend_aot_write_c_dynamic_function_call(FILE *file,
                                               const SZrAotExecIrFunction *functionIr,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount,
                                               TZrUInt32 deoptId);
void backend_aot_write_c_dispatch_loop(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 instructionCount);
void backend_aot_write_c_try(FILE *file, TZrUInt32 handlerIndex);
void backend_aot_write_c_end_try(FILE *file, TZrUInt32 handlerIndex);
void backend_aot_write_c_throw(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 sourceSlot);
void backend_aot_write_c_catch(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_end_finally(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 handlerIndex);
void backend_aot_write_c_set_pending_return(FILE *file,
                                            TZrUInt32 functionFlatIndex,
                                            TZrUInt32 sourceSlot,
                                            TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_set_pending_break(FILE *file, TZrUInt32 functionFlatIndex, TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_set_pending_continue(FILE *file,
                                              TZrUInt32 functionFlatIndex,
                                              TZrUInt32 targetInstructionIndex);

#endif
