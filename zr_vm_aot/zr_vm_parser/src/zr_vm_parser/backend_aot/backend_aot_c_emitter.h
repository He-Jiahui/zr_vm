#ifndef ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H
#define ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

typedef struct SZrAotExecIrFrameLayout SZrAotExecIrFrameLayout;

const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_requires_materialization(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex);
void backend_aot_write_c_direct_primitive_constant(FILE *file,
                                                   TZrUInt32 destinationSlot,
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
void backend_aot_write_c_direct_stack_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_reset_stack_null(FILE *file, TZrUInt32 destinationSlot);
void backend_aot_write_c_direct_reset_stack_null2(FILE *file, TZrUInt32 firstSlot, TZrUInt32 secondSlot);
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
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_add_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_add_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex);
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
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub(FILE *file,
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_sub_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex);
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
void backend_aot_write_c_direct_jump(FILE *file, TZrUInt32 functionIndex, TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if(FILE *file,
                                        TZrUInt32 functionIndex,
                                        TZrUInt32 conditionSlot,
                                        TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if_bool_false(FILE *file,
                                                   TZrUInt32 functionIndex,
                                                   TZrUInt32 conditionSlot,
                                                   TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if_greater_signed(FILE *file,
                                                       TZrUInt32 functionIndex,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot,
                                                       TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if_less_equal_signed(FILE *file,
                                                          TZrUInt32 functionIndex,
                                                          TZrUInt32 leftSlot,
                                                          TZrUInt32 rightSlot,
                                                          TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if_not_equal_signed(FILE *file,
                                                         TZrUInt32 functionIndex,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot,
                                                         TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_jump_if_not_equal_signed_const(FILE *file,
                                                               const SZrFunction *function,
                                                               TZrUInt32 functionIndex,
                                                               TZrUInt32 leftSlot,
                                                               TZrUInt32 constantIndex,
                                                               TZrUInt32 targetInstructionIndex);
void backend_aot_write_c_direct_to_bool(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_publish_exports(FILE *file);
void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports);
void backend_aot_write_c_begin_instruction(FILE *file, TZrUInt32 instructionIndex, TZrUInt32 stepFlags);
void backend_aot_write_c_direct_mark_to_be_closed(FILE *file, TZrUInt32 slotIndex);
void backend_aot_write_c_direct_close_scope(FILE *file, TZrUInt32 cleanupCount);
void backend_aot_write_c_direct_own_unique(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_borrow(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_loan(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_share(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_weak(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_detach(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_upgrade(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_own_release(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_mul_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mul_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex);
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
void backend_aot_write_c_direct_neg(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_neg_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_neg_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
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
void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 leftSlot,
                                                  TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_equal_string(FILE *file,
                                                     const SZrAotExecIrFrameLayout *frameLayout,
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 leftSlot,
                                                     TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal_string(FILE *file,
                                                         const SZrAotExecIrFrameLayout *frameLayout,
                                                         TZrUInt32 destinationSlot,
                                                         TZrUInt32 leftSlot,
                                                         TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_and(FILE *file,
                                            TZrUInt32 destinationSlot,
                                            TZrUInt32 leftSlot,
                                            TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_or(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_equal_bool(FILE *file,
                                                   TZrUInt32 destinationSlot,
                                                   TZrUInt32 leftSlot,
                                                   TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal_bool(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 leftSlot,
                                                       TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_logical_not_bool(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
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
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_mod_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex);
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
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_div_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_div_signed_const(FILE *file,
                                                 const SZrFunction *function,
                                                 TZrUInt32 destinationSlot,
                                                 TZrUInt32 leftSlot,
                                                 TZrUInt32 constantIndex);
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
                                    TZrUInt32 destinationSlot,
                                    TZrUInt32 leftSlot,
                                    TZrUInt32 rightSlot);
void backend_aot_write_c_direct_pow_signed(FILE *file,
                                           TZrUInt32 destinationSlot,
                                           TZrUInt32 leftSlot,
                                           TZrUInt32 rightSlot);
void backend_aot_write_c_direct_pow_unsigned(FILE *file,
                                             TZrUInt32 destinationSlot,
                                             TZrUInt32 leftSlot,
                                             TZrUInt32 rightSlot);
void backend_aot_write_c_direct_pow_float(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 leftSlot,
                                          TZrUInt32 rightSlot);
void backend_aot_write_c_direct_super_array_get_int(FILE *file,
                                                    TZrUInt32 destinationSlot,
                                                    TZrUInt32 receiverSlot,
                                                    TZrUInt32 keySlot);
void backend_aot_write_c_direct_super_array_set_int(FILE *file,
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
                                                             TZrUInt32 targetInstructionIndex);
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
                                                     TZrUInt32 destinationSlot,
                                                     TZrUInt32 functionSlot,
                                                     TZrUInt32 argumentCount,
                                                     TZrUInt32 calleeFlatIndex);
void backend_aot_write_c_direct_function_call(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 functionSlot,
                                              TZrUInt32 argumentCount);
void backend_aot_write_c_dynamic_function_call(FILE *file,
                                               TZrUInt32 destinationSlot,
                                               TZrUInt32 functionSlot,
                                               TZrUInt32 argumentCount);
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
