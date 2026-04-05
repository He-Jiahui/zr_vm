#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_EMITTER_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_EMITTER_H

#include <stdio.h>

#include "backend_aot_internal.h"
#include "backend_aot_llvm_text_flow.h"

typedef struct SZrAotLlvmLoweringContext {
    FILE *file;
    SZrState *state;
    const SZrAotFunctionTable *functionTable;
    const SZrAotFunctionEntry *entry;
    TZrUInt32 *tempCounter;
    TZrUInt32 *callableSlotFunctionIndices;
    const TZrChar *failLabel;
    TZrUInt32 instructionCount;
    TZrBool publishExports;
} SZrAotLlvmLoweringContext;

typedef struct SZrAotLlvmInstructionContext {
    TZrUInt32 instructionIndex;
    TZrUInt32 opcode;
    TZrUInt32 destinationSlot;
    TZrUInt32 operandA1;
    TZrUInt32 operandB1;
    TZrInt32 operandA2;
    const TZrChar *nextLabel;
} SZrAotLlvmInstructionContext;

TZrBool backend_aot_llvm_lower_constant_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_closure_value_subfamily(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_stack_slot_value_subfamily(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_closure_slot_value_family(const SZrAotLlvmLoweringContext *context,
                                                         const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_object_value_family(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_global_value_family(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_creation_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_type_conversion_value_family(const SZrAotLlvmLoweringContext *context,
                                                            const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_member_value_family(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_index_value_family(const SZrAotLlvmLoweringContext *context,
                                                  const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_member_index_value_family(const SZrAotLlvmLoweringContext *context,
                                                         const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_iterator_value_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_meta_access_value_family(const SZrAotLlvmLoweringContext *context,
                                                        const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_ownership_value_family(const SZrAotLlvmLoweringContext *context,
                                                      const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_object_meta_owning_value_family(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_arithmetic_value_family(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_value_instruction(const SZrAotLlvmLoweringContext *context,
                                                 const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_function_call_family(const SZrAotLlvmLoweringContext *context,
                                                    const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_meta_call_family(const SZrAotLlvmLoweringContext *context,
                                                const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_exception_control_family(const SZrAotLlvmLoweringContext *context,
                                                        const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_branch_control_family(const SZrAotLlvmLoweringContext *context,
                                                     const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_call_instruction(const SZrAotLlvmLoweringContext *context,
                                                const SZrAotLlvmInstructionContext *instruction);
TZrBool backend_aot_llvm_lower_control_instruction(const SZrAotLlvmLoweringContext *context,
                                                   const SZrAotLlvmInstructionContext *instruction);

#endif
