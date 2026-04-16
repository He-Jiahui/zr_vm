#ifndef ZR_VM_PARSER_BACKEND_AOT_LLVM_EMITTER_H
#define ZR_VM_PARSER_BACKEND_AOT_LLVM_EMITTER_H

#include <stddef.h>
#include <stdio.h>

#include "backend_aot_internal.h"
#include "backend_aot_llvm_text_flow.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/aot_runtime.h"

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

static ZR_FORCE_INLINE TZrSize backend_aot_llvm_value_field_offset(TZrUInt32 fieldIndex) {
    switch (fieldIndex) {
        case 0:
            return offsetof(SZrTypeValue, type);
        case 1:
            return offsetof(SZrTypeValue, value);
        case 2:
            return offsetof(SZrTypeValue, isGarbageCollectable);
        case 3:
            return offsetof(SZrTypeValue, isNative);
        case 4:
            return offsetof(SZrTypeValue, ownershipKind);
        case 5:
            return offsetof(SZrTypeValue, ownershipControl);
        case 6:
            return offsetof(SZrTypeValue, ownershipWeakRef);
        default:
            ZR_ASSERT(ZR_FALSE && "unexpected SZrTypeValue field index");
            return 0u;
    }
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_type_value_alignment(void) {
    return (TZrUInt32)ZR_ALIGN_SIZE;
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_value_field_alignment(TZrUInt32 fieldIndex) {
    switch (fieldIndex) {
        case 0:
        case 4:
            return 4u;
        case 1:
        case 5:
        case 6:
            return 8u;
        case 2:
        case 3:
            return 1u;
        default:
            ZR_ASSERT(ZR_FALSE && "unexpected SZrTypeValue field index");
            return 1u;
    }
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_byte_offset_pointer(FILE *file,
                                                                           TZrUInt32 *tempCounter,
                                                                           TZrUInt32 basePointerTemp,
                                                                           TZrSize byteOffset) {
    TZrUInt32 pointerTemp;

    if (byteOffset == 0u) {
        return basePointerTemp;
    }

    pointerTemp = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = getelementptr i8, ptr %%t%u, i64 %llu\n",
            (unsigned)pointerTemp,
            (unsigned)basePointerTemp,
            (unsigned long long)byteOffset);
    return pointerTemp;
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_frame_slot_pointer(FILE *file,
                                                                           TZrUInt32 *tempCounter,
                                                                           TZrUInt32 slotIndex) {
    TZrUInt32 slotBasePtrTemp;
    TZrUInt32 slotBaseTemp;

    slotBasePtrTemp = backend_aot_llvm_next_temp(tempCounter);
    slotBaseTemp = backend_aot_llvm_next_temp(tempCounter);
    fprintf(file,
            "  %%t%u = getelementptr %%ZrAotGeneratedFrame, ptr %%frame, i32 0, i32 3\n",
            (unsigned)slotBasePtrTemp);
    fprintf(file, "  %%t%u = load ptr, ptr %%t%u, align 8\n", (unsigned)slotBaseTemp, (unsigned)slotBasePtrTemp);
    return backend_aot_llvm_emit_byte_offset_pointer(file,
                                                     tempCounter,
                                                     slotBaseTemp,
                                                     (TZrSize)slotIndex * sizeof(SZrTypeValueOnStack));
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_stack_value_pointer(FILE *file,
                                                                            TZrUInt32 *tempCounter,
                                                                            TZrUInt32 slotIndex) {
    TZrUInt32 slotPointerTemp = backend_aot_llvm_emit_frame_slot_pointer(file, tempCounter, slotIndex);

    return backend_aot_llvm_emit_byte_offset_pointer(file,
                                                     tempCounter,
                                                     slotPointerTemp,
                                                     offsetof(SZrTypeValueOnStack, value));
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_value_field_pointer(FILE *file,
                                                                            TZrUInt32 *tempCounter,
                                                                            TZrUInt32 valuePointerTemp,
                                                                            TZrUInt32 fieldIndex) {
    return backend_aot_llvm_emit_byte_offset_pointer(file,
                                                     tempCounter,
                                                     valuePointerTemp,
                                                     backend_aot_llvm_value_field_offset(fieldIndex));
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_load_value_type(FILE *file,
                                                                       TZrUInt32 *tempCounter,
                                                                       TZrUInt32 valuePointerTemp) {
    TZrUInt32 typePointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 0);
    TZrUInt32 typeTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = load i32, ptr %%t%u, align %u\n",
            (unsigned)typeTemp,
            (unsigned)typePointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(0));
    return typeTemp;
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_load_value_bits(FILE *file,
                                                                       TZrUInt32 *tempCounter,
                                                                       TZrUInt32 valuePointerTemp) {
    TZrUInt32 bitsPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 1);
    TZrUInt32 bitsTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = load i64, ptr %%t%u, align %u\n",
            (unsigned)bitsTemp,
            (unsigned)bitsPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(1));
    return bitsTemp;
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_load_value_i8_field(FILE *file,
                                                                            TZrUInt32 *tempCounter,
                                                                            TZrUInt32 valuePointerTemp,
                                                                            TZrUInt32 fieldIndex) {
    TZrUInt32 fieldPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, fieldIndex);
    TZrUInt32 fieldTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file, "  %%t%u = load i8, ptr %%t%u, align 1\n", (unsigned)fieldTemp, (unsigned)fieldPointerTemp);
    return fieldTemp;
}

static ZR_FORCE_INLINE TZrUInt32 backend_aot_llvm_emit_load_value_ownership_kind(FILE *file,
                                                                                  TZrUInt32 *tempCounter,
                                                                                  TZrUInt32 valuePointerTemp) {
    TZrUInt32 ownershipPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 4);
    TZrUInt32 ownershipTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = load i32, ptr %%t%u, align %u\n",
            (unsigned)ownershipTemp,
            (unsigned)ownershipPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(4));
    return ownershipTemp;
}

static ZR_FORCE_INLINE void backend_aot_llvm_emit_fast_set_bits(FILE *file,
                                                                TZrUInt32 *tempCounter,
                                                                TZrUInt32 valuePointerTemp,
                                                                const TZrChar *bitsText,
                                                                TZrUInt32 valueType) {
    TZrUInt32 typePointerTemp;
    TZrUInt32 bitsPointerTemp;
    TZrUInt32 isGcPointerTemp;
    TZrUInt32 isNativePointerTemp;
    TZrUInt32 ownershipKindPointerTemp;
    TZrUInt32 ownershipControlPointerTemp;
    TZrUInt32 ownershipWeakRefPointerTemp;

    typePointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 0);
    bitsPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 1);
    isGcPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 2);
    isNativePointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 3);
    ownershipKindPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 4);
    ownershipControlPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 5);
    ownershipWeakRefPointerTemp = backend_aot_llvm_emit_value_field_pointer(file, tempCounter, valuePointerTemp, 6);

    fprintf(file,
            "  store i32 %u, ptr %%t%u, align %u\n",
            (unsigned)valueType,
            (unsigned)typePointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(0));
    fprintf(file,
            "  store i64 %s, ptr %%t%u, align %u\n",
            bitsText,
            (unsigned)bitsPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(1));
    fprintf(file, "  store i8 0, ptr %%t%u, align 1\n", (unsigned)isGcPointerTemp);
    fprintf(file, "  store i8 1, ptr %%t%u, align 1\n", (unsigned)isNativePointerTemp);
    fprintf(file,
            "  store i32 0, ptr %%t%u, align %u\n",
            (unsigned)ownershipKindPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(4));
    fprintf(file,
            "  store ptr null, ptr %%t%u, align %u\n",
            (unsigned)ownershipControlPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(5));
    fprintf(file,
            "  store ptr null, ptr %%t%u, align %u\n",
            (unsigned)ownershipWeakRefPointerTemp,
            (unsigned)backend_aot_llvm_value_field_alignment(6));
}

static ZR_FORCE_INLINE void backend_aot_llvm_emit_fast_set_null(FILE *file,
                                                                TZrUInt32 *tempCounter,
                                                                TZrUInt32 valuePointerTemp) {
    backend_aot_llvm_emit_fast_set_bits(file, tempCounter, valuePointerTemp, "0", ZR_VALUE_TYPE_NULL);
}

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
