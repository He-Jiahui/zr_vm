/* typeof 和类型转换按输入槽数调用运行时 helper，并失效新目标槽的调用出处。 */
#include "backend_aot/backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_pair_slot_type_conversion_call(const SZrAotLlvmLoweringContext *context,
                                                                     const SZrAotLlvmInstructionContext *instruction,
                                                                     const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

typedef enum EZrAotLlvmSpecializedNumericCast {
    ZR_AOT_LLVM_CAST_TO_FLOAT_SIGNED = 0,
    ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED,
    ZR_AOT_LLVM_CAST_TO_INT_FLOAT,
    ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED,
    ZR_AOT_LLVM_CAST_TO_UINT_FLOAT,
    ZR_AOT_LLVM_CAST_TO_UINT_SIGNED
} EZrAotLlvmSpecializedNumericCast;

static TZrUInt32 backend_aot_llvm_emit_cast_type_range(FILE *file,
                                                       TZrUInt32 *tempCounter,
                                                       TZrUInt32 typeTemp,
                                                       TZrUInt32 minType,
                                                       TZrUInt32 maxType) {
    TZrUInt32 lowerTemp = backend_aot_llvm_next_temp(tempCounter);
    TZrUInt32 upperTemp = backend_aot_llvm_next_temp(tempCounter);
    TZrUInt32 resultTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = icmp uge i32 %%t%u, %u\n",
            (unsigned)lowerTemp,
            (unsigned)typeTemp,
            (unsigned)minType);
    fprintf(file,
            "  %%t%u = icmp ule i32 %%t%u, %u\n",
            (unsigned)upperTemp,
            (unsigned)typeTemp,
            (unsigned)maxType);
    fprintf(file,
            "  %%t%u = and i1 %%t%u, %%t%u\n",
            (unsigned)resultTemp,
            (unsigned)lowerTemp,
            (unsigned)upperTemp);
    return resultTemp;
}

static const TZrChar *backend_aot_llvm_specialized_cast_marker(EZrAotLlvmSpecializedNumericCast kind) {
    switch (kind) {
        case ZR_AOT_LLVM_CAST_TO_FLOAT_SIGNED:
            return "zr_aot_llvm_to_float_signed";
        case ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED:
            return "zr_aot_llvm_to_float_unsigned";
        case ZR_AOT_LLVM_CAST_TO_INT_FLOAT:
            return "zr_aot_llvm_to_int_float";
        case ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED:
            return "zr_aot_llvm_to_int_unsigned";
        case ZR_AOT_LLVM_CAST_TO_UINT_FLOAT:
            return "zr_aot_llvm_to_uint_float";
        case ZR_AOT_LLVM_CAST_TO_UINT_SIGNED:
            return "zr_aot_llvm_to_uint_signed";
        default:
            return "zr_aot_llvm_specialized_cast";
    }
}

static TZrUInt32 backend_aot_llvm_specialized_cast_result_type(EZrAotLlvmSpecializedNumericCast kind) {
    switch (kind) {
        case ZR_AOT_LLVM_CAST_TO_FLOAT_SIGNED:
        case ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED:
            return ZR_VALUE_TYPE_DOUBLE;
        case ZR_AOT_LLVM_CAST_TO_UINT_FLOAT:
        case ZR_AOT_LLVM_CAST_TO_UINT_SIGNED:
            return ZR_VALUE_TYPE_UINT64;
        case ZR_AOT_LLVM_CAST_TO_INT_FLOAT:
        case ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED:
        default:
            return ZR_VALUE_TYPE_INT64;
    }
}

/* 与解释器 C 转型一致：浮点越界、NaN 和无穷的结果沿用宿主 fptosi/fptoui。
 * 64 位整数互转只改类型标签，位型与 TO_INT_UNSIGNED 的 2^64 映射相同。 */
static TZrUInt32 backend_aot_llvm_emit_specialized_cast_bits(FILE *file,
                                                             TZrUInt32 *tempCounter,
                                                             TZrUInt32 sourceBitsTemp,
                                                             EZrAotLlvmSpecializedNumericCast kind) {
    TZrUInt32 convertedTemp;
    TZrUInt32 bitsTemp;

    switch (kind) {
        case ZR_AOT_LLVM_CAST_TO_FLOAT_SIGNED:
            convertedTemp = backend_aot_llvm_next_temp(tempCounter);
            bitsTemp = backend_aot_llvm_next_temp(tempCounter);
            fprintf(file,
                    "  %%t%u = sitofp i64 %%t%u to double\n",
                    (unsigned)convertedTemp,
                    (unsigned)sourceBitsTemp);
            fprintf(file,
                    "  %%t%u = bitcast double %%t%u to i64\n",
                    (unsigned)bitsTemp,
                    (unsigned)convertedTemp);
            return bitsTemp;
        case ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED:
            convertedTemp = backend_aot_llvm_next_temp(tempCounter);
            bitsTemp = backend_aot_llvm_next_temp(tempCounter);
            fprintf(file,
                    "  %%t%u = uitofp i64 %%t%u to double\n",
                    (unsigned)convertedTemp,
                    (unsigned)sourceBitsTemp);
            fprintf(file,
                    "  %%t%u = bitcast double %%t%u to i64\n",
                    (unsigned)bitsTemp,
                    (unsigned)convertedTemp);
            return bitsTemp;
        case ZR_AOT_LLVM_CAST_TO_INT_FLOAT:
            convertedTemp = backend_aot_llvm_next_temp(tempCounter);
            bitsTemp = backend_aot_llvm_next_temp(tempCounter);
            fprintf(file,
                    "  %%t%u = bitcast i64 %%t%u to double\n",
                    (unsigned)convertedTemp,
                    (unsigned)sourceBitsTemp);
            fprintf(file,
                    "  %%t%u = fptosi double %%t%u to i64\n",
                    (unsigned)bitsTemp,
                    (unsigned)convertedTemp);
            return bitsTemp;
        case ZR_AOT_LLVM_CAST_TO_UINT_FLOAT:
            convertedTemp = backend_aot_llvm_next_temp(tempCounter);
            bitsTemp = backend_aot_llvm_next_temp(tempCounter);
            fprintf(file,
                    "  %%t%u = bitcast i64 %%t%u to double\n",
                    (unsigned)convertedTemp,
                    (unsigned)sourceBitsTemp);
            fprintf(file,
                    "  %%t%u = fptoui double %%t%u to i64\n",
                    (unsigned)bitsTemp,
                    (unsigned)convertedTemp);
            return bitsTemp;
        case ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED:
        case ZR_AOT_LLVM_CAST_TO_UINT_SIGNED:
        default:
            return sourceBitsTemp;
    }
}

static TZrBool backend_aot_llvm_lower_specialized_numeric_cast(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction,
                                                               EZrAotLlvmSpecializedNumericCast kind) {
    TZrUInt32 destinationValueTemp;
    TZrUInt32 sourceValueTemp;
    TZrUInt32 sourceTypeTemp;
    TZrUInt32 sourceBitsTemp;
    TZrUInt32 sourceOkTemp;
    TZrUInt32 resultBitsTemp;
    TZrUInt32 labelSeed;
    TZrChar fastLabel[96];
    TZrChar bitsBuffer[32];
    TZrUInt32 minType;
    TZrUInt32 maxType;

    if (kind == ZR_AOT_LLVM_CAST_TO_INT_FLOAT || kind == ZR_AOT_LLVM_CAST_TO_UINT_FLOAT) {
        minType = ZR_VALUE_TYPE_FLOAT;
        maxType = ZR_VALUE_TYPE_DOUBLE;
    } else if (kind == ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED || kind == ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED) {
        minType = ZR_VALUE_TYPE_UINT8;
        maxType = ZR_VALUE_TYPE_UINT64;
    } else {
        minType = ZR_VALUE_TYPE_INT8;
        maxType = ZR_VALUE_TYPE_INT64;
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    fprintf(context->file, "  ; %s\n", backend_aot_llvm_specialized_cast_marker(kind));
    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    sourceValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                context->tempCounter,
                                                                instruction->operandA1);
    sourceTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, sourceValueTemp);
    sourceBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, sourceValueTemp);
    sourceOkTemp = backend_aot_llvm_emit_cast_type_range(context->file,
                                                         context->tempCounter,
                                                         sourceTypeTemp,
                                                         minType,
                                                         maxType);

    labelSeed = backend_aot_llvm_next_temp(context->tempCounter);
    snprintf(fastLabel, sizeof(fastLabel), "zr_aot_cast_fast_%u", (unsigned)labelSeed);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)sourceOkTemp,
            fastLabel,
            context->failLabel);
    fprintf(context->file, "%s:\n", fastLabel);
    resultBitsTemp = backend_aot_llvm_emit_specialized_cast_bits(context->file,
                                                                 context->tempCounter,
                                                                 sourceBitsTemp,
                                                                 kind);
    snprintf(bitsBuffer, sizeof(bitsBuffer), "%%t%u", (unsigned)resultBitsTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        bitsBuffer,
                                        backend_aot_llvm_specialized_cast_result_type(kind));
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_triple_slot_type_conversion_call(const SZrAotLlvmLoweringContext *context,
                                                                       const SZrAotLlvmInstructionContext *instruction,
                                                                       const TZrChar *helperName) {
    TZrChar argsBuffer[256];

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             helperName,
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

TZrBool backend_aot_llvm_lower_type_conversion_value_family(const SZrAotLlvmLoweringContext *context,
                                                            const SZrAotLlvmInstructionContext *instruction) {
    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(TYPEOF):
            return backend_aot_llvm_lower_pair_slot_type_conversion_call(context,
                                                                         instruction,
                                                                         "ZrLibrary_AotRuntime_TypeOf");
        case ZR_INSTRUCTION_ENUM(TO_BOOL):
            return backend_aot_llvm_lower_pair_slot_type_conversion_call(context,
                                                                         instruction,
                                                                         "ZrLibrary_AotRuntime_ToBool");
        case ZR_INSTRUCTION_ENUM(TO_UINT):
            return backend_aot_llvm_lower_pair_slot_type_conversion_call(context,
                                                                         instruction,
                                                                         "ZrLibrary_AotRuntime_ToUInt");
        case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            return backend_aot_llvm_lower_pair_slot_type_conversion_call(context,
                                                                         instruction,
                                                                         "ZrLibrary_AotRuntime_ToFloat");
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_SIGNED):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_FLOAT_SIGNED);
        case ZR_INSTRUCTION_ENUM(TO_FLOAT_UNSIGNED):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_FLOAT_UNSIGNED);
        case ZR_INSTRUCTION_ENUM(TO_INT_FLOAT):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_INT_FLOAT);
        case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_INT_UNSIGNED);
        case ZR_INSTRUCTION_ENUM(TO_UINT_FLOAT):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_UINT_FLOAT);
        case ZR_INSTRUCTION_ENUM(TO_UINT_SIGNED):
            return backend_aot_llvm_lower_specialized_numeric_cast(context,
                                                                   instruction,
                                                                   ZR_AOT_LLVM_CAST_TO_UINT_SIGNED);
        case ZR_INSTRUCTION_ENUM(TO_OBJECT):
            return backend_aot_llvm_lower_triple_slot_type_conversion_call(context,
                                                                           instruction,
                                                                           "ZrLibrary_AotRuntime_ToObject");
        case ZR_INSTRUCTION_ENUM(TO_STRUCT):
            return backend_aot_llvm_lower_triple_slot_type_conversion_call(context,
                                                                           instruction,
                                                                           "ZrLibrary_AotRuntime_ToStruct");
        default:
            return ZR_FALSE;
    }
}
