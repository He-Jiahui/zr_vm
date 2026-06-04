#ifndef ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H
#define ZR_VM_PARSER_BACKEND_AOT_C_EMITTER_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

const SZrTypeValue *backend_aot_c_get_constant_value(const SZrFunction *function, TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_requires_materialization(SZrState *state,
                                                        const SZrFunction *function,
                                                        TZrInt32 constantIndex);
TZrBool backend_aot_c_constant_can_emit_immediate(const SZrFunction *function, TZrInt32 constantIndex);
void backend_aot_write_c_immediate_constant_copy(FILE *file,
                                                 TZrUInt32 destinationSlot,
                                                 const SZrTypeValue *constantValue);
void backend_aot_write_c_direct_constant_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 constantIndex);
void backend_aot_write_c_direct_callable_constant(FILE *file,
                                                  TZrUInt32 destinationSlot,
                                                  TZrUInt32 constantIndex,
                                                  TZrUInt32 callableFlatIndex);
void backend_aot_write_c_direct_stack_copy(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
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
void backend_aot_write_c_direct_to_int(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_float_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_int_unsigned(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_float(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_to_uint_signed(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_return(FILE *file, TZrUInt32 sourceSlot);
void backend_aot_write_c_tail_return(FILE *file, TZrUInt32 sourceSlot, TZrBool publishExports);
void backend_aot_write_c_begin_instruction(FILE *file, TZrUInt32 instructionIndex, TZrUInt32 stepFlags);
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
void backend_aot_write_c_direct_meta_get(FILE *file,
                                         TZrUInt32 destinationSlot,
                                         TZrUInt32 receiverSlot,
                                         TZrUInt32 memberId);
void backend_aot_write_c_direct_meta_set(FILE *file,
                                         TZrUInt32 receiverAndResultSlot,
                                         TZrUInt32 assignedValueSlot,
                                         TZrUInt32 memberId);
void backend_aot_write_c_direct_meta_get_cached(FILE *file,
                                                TZrUInt32 destinationSlot,
                                                TZrUInt32 receiverSlot,
                                                TZrUInt32 cacheIndex);
void backend_aot_write_c_direct_meta_set_cached(FILE *file,
                                                TZrUInt32 receiverAndResultSlot,
                                                TZrUInt32 assignedValueSlot,
                                                TZrUInt32 cacheIndex);
void backend_aot_write_c_direct_meta_get_static_cached(FILE *file,
                                                       TZrUInt32 destinationSlot,
                                                       TZrUInt32 receiverSlot,
                                                       TZrUInt32 cacheIndex);
void backend_aot_write_c_direct_meta_set_static_cached(FILE *file,
                                                       TZrUInt32 receiverAndResultSlot,
                                                       TZrUInt32 assignedValueSlot,
                                                       TZrUInt32 cacheIndex);
void backend_aot_write_c_direct_logical_equal(FILE *file,
                                              TZrUInt32 destinationSlot,
                                              TZrUInt32 leftSlot,
                                              TZrUInt32 rightSlot);
void backend_aot_write_c_direct_logical_not_equal(FILE *file,
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
void backend_aot_write_c_direct_to_string(FILE *file, TZrUInt32 destinationSlot, TZrUInt32 sourceSlot);
void backend_aot_write_c_direct_meta_call(FILE *file,
                                          TZrUInt32 destinationSlot,
                                          TZrUInt32 receiverSlot,
                                          TZrUInt32 argumentCount);
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
