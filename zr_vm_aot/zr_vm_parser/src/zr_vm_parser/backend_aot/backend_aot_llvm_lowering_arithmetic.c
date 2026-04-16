#include "backend_aot_llvm_emitter.h"

static TZrBool backend_aot_llvm_lower_binary_value_instruction(const SZrAotLlvmLoweringContext *context,
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

static TZrBool backend_aot_llvm_lower_unary_value_instruction(const SZrAotLlvmLoweringContext *context,
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

static TZrUInt32 backend_aot_llvm_emit_type_equal_check(FILE *file,
                                                        TZrUInt32 *tempCounter,
                                                        TZrUInt32 typeTemp,
                                                        TZrUInt32 valueType) {
    TZrUInt32 resultTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = icmp eq i32 %%t%u, %u\n",
            (unsigned)resultTemp,
            (unsigned)typeTemp,
            (unsigned)valueType);
    return resultTemp;
}

static TZrUInt32 backend_aot_llvm_emit_type_range_check(FILE *file,
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

static TZrUInt32 backend_aot_llvm_emit_or_condition(FILE *file,
                                                    TZrUInt32 *tempCounter,
                                                    TZrUInt32 leftTemp,
                                                    TZrUInt32 rightTemp) {
    TZrUInt32 resultTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = or i1 %%t%u, %%t%u\n",
            (unsigned)resultTemp,
            (unsigned)leftTemp,
            (unsigned)rightTemp);
    return resultTemp;
}

static TZrUInt32 backend_aot_llvm_emit_and_condition(FILE *file,
                                                     TZrUInt32 *tempCounter,
                                                     TZrUInt32 leftTemp,
                                                     TZrUInt32 rightTemp) {
    TZrUInt32 resultTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = and i1 %%t%u, %%t%u\n",
            (unsigned)resultTemp,
            (unsigned)leftTemp,
            (unsigned)rightTemp);
    return resultTemp;
}

static TZrUInt32 backend_aot_llvm_emit_bool_bits_to_i64(FILE *file,
                                                        TZrUInt32 *tempCounter,
                                                        TZrUInt32 bitsTemp) {
    TZrUInt32 nonZeroTemp = backend_aot_llvm_next_temp(tempCounter);
    TZrUInt32 intTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = icmp ne i64 %%t%u, 0\n",
            (unsigned)nonZeroTemp,
            (unsigned)bitsTemp);
    fprintf(file,
            "  %%t%u = zext i1 %%t%u to i64\n",
            (unsigned)intTemp,
            (unsigned)nonZeroTemp);
    return intTemp;
}

static TZrUInt32 backend_aot_llvm_emit_numeric_bits_as_double(FILE *file,
                                                              TZrUInt32 *tempCounter,
                                                              TZrUInt32 bitsTemp,
                                                              TZrUInt32 isFloatTemp,
                                                              TZrUInt32 isUnsignedTemp,
                                                              TZrUInt32 isBoolTemp) {
    TZrUInt32 boolIntTemp;
    TZrUInt32 boolDoubleTemp;
    TZrUInt32 signedDoubleTemp;
    TZrUInt32 unsignedDoubleTemp;
    TZrUInt32 floatDoubleTemp;
    TZrUInt32 boolOrSignedTemp;
    TZrUInt32 nonFloatTemp;
    TZrUInt32 resultTemp;

    boolIntTemp = backend_aot_llvm_emit_bool_bits_to_i64(file, tempCounter, bitsTemp);
    boolDoubleTemp = backend_aot_llvm_next_temp(tempCounter);
    signedDoubleTemp = backend_aot_llvm_next_temp(tempCounter);
    unsignedDoubleTemp = backend_aot_llvm_next_temp(tempCounter);
    floatDoubleTemp = backend_aot_llvm_next_temp(tempCounter);
    boolOrSignedTemp = backend_aot_llvm_next_temp(tempCounter);
    nonFloatTemp = backend_aot_llvm_next_temp(tempCounter);
    resultTemp = backend_aot_llvm_next_temp(tempCounter);

    fprintf(file,
            "  %%t%u = uitofp i64 %%t%u to double\n",
            (unsigned)boolDoubleTemp,
            (unsigned)boolIntTemp);
    fprintf(file,
            "  %%t%u = sitofp i64 %%t%u to double\n",
            (unsigned)signedDoubleTemp,
            (unsigned)bitsTemp);
    fprintf(file,
            "  %%t%u = uitofp i64 %%t%u to double\n",
            (unsigned)unsignedDoubleTemp,
            (unsigned)bitsTemp);
    fprintf(file,
            "  %%t%u = bitcast i64 %%t%u to double\n",
            (unsigned)floatDoubleTemp,
            (unsigned)bitsTemp);
    fprintf(file,
            "  %%t%u = select i1 %%t%u, double %%t%u, double %%t%u\n",
            (unsigned)boolOrSignedTemp,
            (unsigned)isBoolTemp,
            (unsigned)boolDoubleTemp,
            (unsigned)signedDoubleTemp);
    fprintf(file,
            "  %%t%u = select i1 %%t%u, double %%t%u, double %%t%u\n",
            (unsigned)nonFloatTemp,
            (unsigned)isUnsignedTemp,
            (unsigned)unsignedDoubleTemp,
            (unsigned)boolOrSignedTemp);
    fprintf(file,
            "  %%t%u = select i1 %%t%u, double %%t%u, double %%t%u\n",
            (unsigned)resultTemp,
            (unsigned)isFloatTemp,
            (unsigned)floatDoubleTemp,
            (unsigned)nonFloatTemp);
    return resultTemp;
}

static void backend_aot_llvm_format_temp_ref(TZrChar *buffer, TZrSize bufferSize, TZrUInt32 temp) {
    if (buffer == ZR_NULL || bufferSize == 0u) {
        return;
    }

    snprintf(buffer, (size_t)bufferSize, "%%t%u", (unsigned)temp);
}

typedef enum EZrAotLlvmIntegerFamilyKind {
    ZR_AOT_LLVM_INTEGER_FAMILY_ANY = 0,
    ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED = 1,
    ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED = 2
} EZrAotLlvmIntegerFamilyKind;

static TZrUInt32 backend_aot_llvm_integer_family_result_type(EZrAotLlvmIntegerFamilyKind familyKind) {
    return familyKind == ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED ? ZR_VALUE_TYPE_UINT64 : ZR_VALUE_TYPE_INT64;
}

static const TZrChar *backend_aot_llvm_add_integer_family_helper_name(EZrAotLlvmIntegerFamilyKind familyKind,
                                                                      TZrBool isConst) {
    switch (familyKind) {
        case ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED:
            return isConst ? "ZrLibrary_AotRuntime_AddSignedConst" : "ZrLibrary_AotRuntime_AddSigned";
        case ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED:
            return isConst ? "ZrLibrary_AotRuntime_AddUnsignedConst" : "ZrLibrary_AotRuntime_AddUnsigned";
        case ZR_AOT_LLVM_INTEGER_FAMILY_ANY:
        default:
            return isConst ? "ZrLibrary_AotRuntime_AddIntConst" : "ZrLibrary_AotRuntime_AddInt";
    }
}

static TZrUInt32 backend_aot_llvm_emit_integer_family_check(FILE *file,
                                                            TZrUInt32 *tempCounter,
                                                            TZrUInt32 typeTemp,
                                                            EZrAotLlvmIntegerFamilyKind familyKind) {
    switch (familyKind) {
        case ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED:
            return backend_aot_llvm_emit_type_range_check(file,
                                                          tempCounter,
                                                          typeTemp,
                                                          ZR_VALUE_TYPE_INT8,
                                                          ZR_VALUE_TYPE_INT64);
        case ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED:
            return backend_aot_llvm_emit_type_range_check(file,
                                                          tempCounter,
                                                          typeTemp,
                                                          ZR_VALUE_TYPE_UINT8,
                                                          ZR_VALUE_TYPE_UINT64);
        case ZR_AOT_LLVM_INTEGER_FAMILY_ANY:
        default: {
            TZrUInt32 signedTemp = backend_aot_llvm_emit_type_range_check(file,
                                                                          tempCounter,
                                                                          typeTemp,
                                                                          ZR_VALUE_TYPE_INT8,
                                                                          ZR_VALUE_TYPE_INT64);
            TZrUInt32 unsignedTemp = backend_aot_llvm_emit_type_range_check(file,
                                                                            tempCounter,
                                                                            typeTemp,
                                                                            ZR_VALUE_TYPE_UINT8,
                                                                            ZR_VALUE_TYPE_UINT64);
            return backend_aot_llvm_emit_or_condition(file, tempCounter, signedTemp, unsignedTemp);
        }
    }
}

static TZrBool backend_aot_llvm_add_integer_const_literal(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction,
                                                          EZrAotLlvmIntegerFamilyKind familyKind,
                                                          TZrChar *literalBuffer,
                                                          TZrSize literalBufferSize) {
    const SZrTypeValue *constantValue;

    if (context == ZR_NULL || instruction == ZR_NULL || literalBuffer == ZR_NULL || literalBufferSize == 0u ||
        instruction->operandB1 >= context->entry->function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &context->entry->function->constantValueList[instruction->operandB1];
    switch (familyKind) {
        case ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED:
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
                snprintf(literalBuffer,
                         (size_t)literalBufferSize,
                         "%lld",
                         (long long)constantValue->value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            return ZR_FALSE;
        case ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED:
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
                snprintf(literalBuffer,
                         (size_t)literalBufferSize,
                         "%llu",
                         (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
                return ZR_TRUE;
            }
            return ZR_FALSE;
        case ZR_AOT_LLVM_INTEGER_FAMILY_ANY:
        default:
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(constantValue->type)) {
                snprintf(literalBuffer,
                         (size_t)literalBufferSize,
                         "%lld",
                         (long long)constantValue->value.nativeObject.nativeInt64);
                return ZR_TRUE;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)) {
                snprintf(literalBuffer,
                         (size_t)literalBufferSize,
                         "%llu",
                         (unsigned long long)constantValue->value.nativeObject.nativeUInt64);
                return ZR_TRUE;
            }
            if (ZR_VALUE_IS_TYPE_BOOL(constantValue->type)) {
                snprintf(literalBuffer,
                         (size_t)literalBufferSize,
                         "%u",
                         constantValue->value.nativeObject.nativeBool != 0 ? 1u : 0u);
                return ZR_TRUE;
            }
            return ZR_FALSE;
    }
}

static TZrBool backend_aot_llvm_lower_add_integer_family_instruction(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction,
        EZrAotLlvmIntegerFamilyKind familyKind) {
    TZrUInt32 destinationValueTemp;
    TZrUInt32 leftValueTemp;
    TZrUInt32 rightValueTemp;
    TZrUInt32 leftTypeTemp;
    TZrUInt32 rightTypeTemp;
    TZrUInt32 leftBitsTemp;
    TZrUInt32 rightBitsTemp;
    TZrUInt32 leftIntTemp;
    TZrUInt32 rightIntTemp;
    TZrUInt32 bothIntTemp;
    TZrUInt32 sumTemp;
    TZrUInt32 labelSeed;
    TZrChar fastLabel[96];
    TZrChar sumBuffer[32];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    leftValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                              context->tempCounter,
                                                              instruction->operandA1);
    rightValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                               context->tempCounter,
                                                               instruction->operandB1);
    leftTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, leftValueTemp);
    rightTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, rightValueTemp);
    leftBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, leftValueTemp);
    rightBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, rightValueTemp);
    leftIntTemp = backend_aot_llvm_emit_integer_family_check(context->file,
                                                             context->tempCounter,
                                                             leftTypeTemp,
                                                             familyKind);
    rightIntTemp = backend_aot_llvm_emit_integer_family_check(context->file,
                                                              context->tempCounter,
                                                              rightTypeTemp,
                                                              familyKind);
    bothIntTemp = backend_aot_llvm_emit_and_condition(context->file,
                                                      context->tempCounter,
                                                      leftIntTemp,
                                                      rightIntTemp);

    labelSeed = backend_aot_llvm_next_temp(context->tempCounter);
    snprintf(fastLabel, sizeof(fastLabel), "zr_aot_add_int_fast_%u", (unsigned)labelSeed);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)bothIntTemp,
            fastLabel,
            context->failLabel);

    fprintf(context->file, "%s:\n", fastLabel);
    sumTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = add i64 %%t%u, %%t%u\n",
            (unsigned)sumTemp,
            (unsigned)leftBitsTemp,
            (unsigned)rightBitsTemp);
    backend_aot_llvm_format_temp_ref(sumBuffer, sizeof(sumBuffer), sumTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        sumBuffer,
                                        backend_aot_llvm_integer_family_result_type(familyKind));
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_add_integer_const_family_instruction(
        const SZrAotLlvmLoweringContext *context,
        const SZrAotLlvmInstructionContext *instruction,
        EZrAotLlvmIntegerFamilyKind familyKind) {
    TZrUInt32 destinationValueTemp;
    TZrUInt32 leftValueTemp;
    TZrUInt32 leftTypeTemp;
    TZrUInt32 leftBitsTemp;
    TZrUInt32 leftIntTemp;
    TZrUInt32 sumTemp;
    TZrUInt32 labelSeed;
    TZrChar fastLabel[96];
    TZrChar literalBuffer[64];
    TZrChar sumBuffer[32];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!backend_aot_llvm_add_integer_const_literal(context,
                                                    instruction,
                                                    familyKind,
                                                    literalBuffer,
                                                    sizeof(literalBuffer))) {
        return backend_aot_llvm_lower_binary_value_instruction(context,
                                                               instruction,
                                                               backend_aot_llvm_add_integer_family_helper_name(
                                                                       familyKind,
                                                                       ZR_TRUE));
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    leftValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                              context->tempCounter,
                                                              instruction->operandA1);
    leftTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, leftValueTemp);
    leftBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, leftValueTemp);
    leftIntTemp = backend_aot_llvm_emit_integer_family_check(context->file,
                                                             context->tempCounter,
                                                             leftTypeTemp,
                                                             familyKind);

    labelSeed = backend_aot_llvm_next_temp(context->tempCounter);
    snprintf(fastLabel, sizeof(fastLabel), "zr_aot_add_int_const_fast_%u", (unsigned)labelSeed);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)leftIntTemp,
            fastLabel,
            context->failLabel);

    fprintf(context->file, "%s:\n", fastLabel);
    sumTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = add i64 %%t%u, %s\n",
            (unsigned)sumTemp,
            (unsigned)leftBitsTemp,
            literalBuffer);
    backend_aot_llvm_format_temp_ref(sumBuffer, sizeof(sumBuffer), sumTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        sumBuffer,
                                        backend_aot_llvm_integer_family_result_type(familyKind));
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);
    return ZR_TRUE;
}

static TZrBool backend_aot_llvm_lower_add_int_instruction(const SZrAotLlvmLoweringContext *context,
                                                          const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_family_instruction(context,
                                                                 instruction,
                                                                 ZR_AOT_LLVM_INTEGER_FAMILY_ANY);
}

static TZrBool backend_aot_llvm_lower_add_signed_instruction(const SZrAotLlvmLoweringContext *context,
                                                             const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_family_instruction(context,
                                                                 instruction,
                                                                 ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED);
}

static TZrBool backend_aot_llvm_lower_add_unsigned_instruction(const SZrAotLlvmLoweringContext *context,
                                                               const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_family_instruction(context,
                                                                 instruction,
                                                                 ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED);
}

static TZrBool backend_aot_llvm_lower_add_int_const_instruction(const SZrAotLlvmLoweringContext *context,
                                                                const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_const_family_instruction(context,
                                                                       instruction,
                                                                       ZR_AOT_LLVM_INTEGER_FAMILY_ANY);
}

static TZrBool backend_aot_llvm_lower_add_signed_const_instruction(const SZrAotLlvmLoweringContext *context,
                                                                   const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_const_family_instruction(context,
                                                                       instruction,
                                                                       ZR_AOT_LLVM_INTEGER_FAMILY_SIGNED);
}

static TZrBool backend_aot_llvm_lower_add_unsigned_const_instruction(const SZrAotLlvmLoweringContext *context,
                                                                     const SZrAotLlvmInstructionContext *instruction) {
    return backend_aot_llvm_lower_add_integer_const_family_instruction(context,
                                                                       instruction,
                                                                       ZR_AOT_LLVM_INTEGER_FAMILY_UNSIGNED);
}

static TZrBool backend_aot_llvm_lower_add_instruction(const SZrAotLlvmLoweringContext *context,
                                                      const SZrAotLlvmInstructionContext *instruction) {
    TZrUInt32 destinationValueTemp;
    TZrUInt32 leftValueTemp;
    TZrUInt32 rightValueTemp;
    TZrUInt32 leftTypeTemp;
    TZrUInt32 rightTypeTemp;
    TZrUInt32 leftBitsTemp;
    TZrUInt32 rightBitsTemp;
    TZrUInt32 leftSignedTemp;
    TZrUInt32 leftUnsignedTemp;
    TZrUInt32 leftFloatTemp;
    TZrUInt32 leftBoolTemp;
    TZrUInt32 leftIntTemp;
    TZrUInt32 leftNumericTemp;
    TZrUInt32 leftNumericOrBoolTemp;
    TZrUInt32 rightSignedTemp;
    TZrUInt32 rightUnsignedTemp;
    TZrUInt32 rightFloatTemp;
    TZrUInt32 rightBoolTemp;
    TZrUInt32 rightIntTemp;
    TZrUInt32 rightNumericTemp;
    TZrUInt32 rightNumericOrBoolTemp;
    TZrUInt32 bothNumericOrBoolTemp;
    TZrUInt32 anyFloatTemp;
    TZrUInt32 anySignedTemp;
    TZrUInt32 anyBoolTemp;
    TZrUInt32 anySignedOrBoolTemp;
    TZrUInt32 leftDoubleTemp;
    TZrUInt32 rightDoubleTemp;
    TZrUInt32 floatSumTemp;
    TZrUInt32 floatBitsTemp;
    TZrUInt32 leftBoolIntTemp;
    TZrUInt32 rightBoolIntTemp;
    TZrUInt32 leftIntValueTemp;
    TZrUInt32 rightIntValueTemp;
    TZrUInt32 intSumTemp;
    TZrUInt32 labelSeed;
    TZrChar fastLabel[96];
    TZrChar floatLabel[96];
    TZrChar intLabel[96];
    TZrChar signedResultLabel[96];
    TZrChar unsignedResultLabel[96];
    TZrChar fallbackLabel[96];
    TZrChar argsBuffer[256];
    TZrChar floatBitsBuffer[32];
    TZrChar intSumBuffer[32];

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    backend_aot_set_callable_slot_function_index(context->callableSlotFunctionIndices,
                                                 context->entry->function,
                                                 instruction->destinationSlot,
                                                 ZR_AOT_INVALID_FUNCTION_INDEX);
    destinationValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                                     context->tempCounter,
                                                                     instruction->destinationSlot);
    leftValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                              context->tempCounter,
                                                              instruction->operandA1);
    rightValueTemp = backend_aot_llvm_emit_stack_value_pointer(context->file,
                                                               context->tempCounter,
                                                               instruction->operandB1);
    leftTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, leftValueTemp);
    rightTypeTemp = backend_aot_llvm_emit_load_value_type(context->file, context->tempCounter, rightValueTemp);
    leftBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, leftValueTemp);
    rightBitsTemp = backend_aot_llvm_emit_load_value_bits(context->file, context->tempCounter, rightValueTemp);

    leftSignedTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                            context->tempCounter,
                                                            leftTypeTemp,
                                                            ZR_VALUE_TYPE_INT8,
                                                            ZR_VALUE_TYPE_INT64);
    leftUnsignedTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                              context->tempCounter,
                                                              leftTypeTemp,
                                                              ZR_VALUE_TYPE_UINT8,
                                                              ZR_VALUE_TYPE_UINT64);
    leftFloatTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                           context->tempCounter,
                                                           leftTypeTemp,
                                                           ZR_VALUE_TYPE_FLOAT,
                                                           ZR_VALUE_TYPE_DOUBLE);
    leftBoolTemp = backend_aot_llvm_emit_type_equal_check(context->file,
                                                          context->tempCounter,
                                                          leftTypeTemp,
                                                          ZR_VALUE_TYPE_BOOL);
    leftIntTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                     context->tempCounter,
                                                     leftSignedTemp,
                                                     leftUnsignedTemp);
    leftNumericTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                         context->tempCounter,
                                                         leftIntTemp,
                                                         leftFloatTemp);
    leftNumericOrBoolTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                               context->tempCounter,
                                                               leftNumericTemp,
                                                               leftBoolTemp);

    rightSignedTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                             context->tempCounter,
                                                             rightTypeTemp,
                                                             ZR_VALUE_TYPE_INT8,
                                                             ZR_VALUE_TYPE_INT64);
    rightUnsignedTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                               context->tempCounter,
                                                               rightTypeTemp,
                                                               ZR_VALUE_TYPE_UINT8,
                                                               ZR_VALUE_TYPE_UINT64);
    rightFloatTemp = backend_aot_llvm_emit_type_range_check(context->file,
                                                            context->tempCounter,
                                                            rightTypeTemp,
                                                            ZR_VALUE_TYPE_FLOAT,
                                                            ZR_VALUE_TYPE_DOUBLE);
    rightBoolTemp = backend_aot_llvm_emit_type_equal_check(context->file,
                                                           context->tempCounter,
                                                           rightTypeTemp,
                                                           ZR_VALUE_TYPE_BOOL);
    rightIntTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                      context->tempCounter,
                                                      rightSignedTemp,
                                                      rightUnsignedTemp);
    rightNumericTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                          context->tempCounter,
                                                          rightIntTemp,
                                                          rightFloatTemp);
    rightNumericOrBoolTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                                context->tempCounter,
                                                                rightNumericTemp,
                                                                rightBoolTemp);

    bothNumericOrBoolTemp = backend_aot_llvm_emit_and_condition(context->file,
                                                                context->tempCounter,
                                                                leftNumericOrBoolTemp,
                                                                rightNumericOrBoolTemp);
    anyFloatTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                      context->tempCounter,
                                                      leftFloatTemp,
                                                      rightFloatTemp);
    anySignedTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                       context->tempCounter,
                                                       leftSignedTemp,
                                                       rightSignedTemp);
    anyBoolTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                     context->tempCounter,
                                                     leftBoolTemp,
                                                     rightBoolTemp);
    anySignedOrBoolTemp = backend_aot_llvm_emit_or_condition(context->file,
                                                             context->tempCounter,
                                                             anySignedTemp,
                                                             anyBoolTemp);

    labelSeed = backend_aot_llvm_next_temp(context->tempCounter);
    snprintf(fastLabel, sizeof(fastLabel), "zr_aot_add_fast_%u", (unsigned)labelSeed);
    snprintf(floatLabel, sizeof(floatLabel), "zr_aot_add_float_%u", (unsigned)labelSeed);
    snprintf(intLabel, sizeof(intLabel), "zr_aot_add_int_%u", (unsigned)labelSeed);
    snprintf(signedResultLabel, sizeof(signedResultLabel), "zr_aot_add_signed_%u", (unsigned)labelSeed);
    snprintf(unsignedResultLabel, sizeof(unsignedResultLabel), "zr_aot_add_unsigned_%u", (unsigned)labelSeed);
    snprintf(fallbackLabel, sizeof(fallbackLabel), "zr_aot_add_fallback_%u", (unsigned)labelSeed);

    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)bothNumericOrBoolTemp,
            fastLabel,
            fallbackLabel);

    fprintf(context->file, "%s:\n", fastLabel);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)anyFloatTemp,
            floatLabel,
            intLabel);

    fprintf(context->file, "%s:\n", floatLabel);
    leftDoubleTemp = backend_aot_llvm_emit_numeric_bits_as_double(context->file,
                                                                  context->tempCounter,
                                                                  leftBitsTemp,
                                                                  leftFloatTemp,
                                                                  leftUnsignedTemp,
                                                                  leftBoolTemp);
    rightDoubleTemp = backend_aot_llvm_emit_numeric_bits_as_double(context->file,
                                                                   context->tempCounter,
                                                                   rightBitsTemp,
                                                                   rightFloatTemp,
                                                                   rightUnsignedTemp,
                                                                   rightBoolTemp);
    floatSumTemp = backend_aot_llvm_next_temp(context->tempCounter);
    floatBitsTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = fadd double %%t%u, %%t%u\n",
            (unsigned)floatSumTemp,
            (unsigned)leftDoubleTemp,
            (unsigned)rightDoubleTemp);
    fprintf(context->file,
            "  %%t%u = bitcast double %%t%u to i64\n",
            (unsigned)floatBitsTemp,
            (unsigned)floatSumTemp);
    backend_aot_llvm_format_temp_ref(floatBitsBuffer, sizeof(floatBitsBuffer), floatBitsTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        floatBitsBuffer,
                                        ZR_VALUE_TYPE_DOUBLE);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", intLabel);
    leftBoolIntTemp = backend_aot_llvm_emit_bool_bits_to_i64(context->file, context->tempCounter, leftBitsTemp);
    rightBoolIntTemp = backend_aot_llvm_emit_bool_bits_to_i64(context->file, context->tempCounter, rightBitsTemp);
    leftIntValueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    rightIntValueTemp = backend_aot_llvm_next_temp(context->tempCounter);
    intSumTemp = backend_aot_llvm_next_temp(context->tempCounter);
    fprintf(context->file,
            "  %%t%u = select i1 %%t%u, i64 %%t%u, i64 %%t%u\n",
            (unsigned)leftIntValueTemp,
            (unsigned)leftBoolTemp,
            (unsigned)leftBoolIntTemp,
            (unsigned)leftBitsTemp);
    fprintf(context->file,
            "  %%t%u = select i1 %%t%u, i64 %%t%u, i64 %%t%u\n",
            (unsigned)rightIntValueTemp,
            (unsigned)rightBoolTemp,
            (unsigned)rightBoolIntTemp,
            (unsigned)rightBitsTemp);
    fprintf(context->file,
            "  %%t%u = add i64 %%t%u, %%t%u\n",
            (unsigned)intSumTemp,
            (unsigned)leftIntValueTemp,
            (unsigned)rightIntValueTemp);
    fprintf(context->file,
            "  br i1 %%t%u, label %%%s, label %%%s\n",
            (unsigned)anySignedOrBoolTemp,
            signedResultLabel,
            unsignedResultLabel);

    fprintf(context->file, "%s:\n", signedResultLabel);
    backend_aot_llvm_format_temp_ref(intSumBuffer, sizeof(intSumBuffer), intSumTemp);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        intSumBuffer,
                                        ZR_VALUE_TYPE_INT64);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", unsignedResultLabel);
    backend_aot_llvm_emit_fast_set_bits(context->file,
                                        context->tempCounter,
                                        destinationValueTemp,
                                        intSumBuffer,
                                        ZR_VALUE_TYPE_UINT64);
    fprintf(context->file, "  br label %%%s\n", instruction->nextLabel);

    fprintf(context->file, "%s:\n", fallbackLabel);
    snprintf(argsBuffer,
             sizeof(argsBuffer),
             "ptr %%state, ptr %%frame, i32 %u, i32 %u, i32 %u",
             (unsigned)instruction->destinationSlot,
             (unsigned)instruction->operandA1,
             (unsigned)instruction->operandB1);
    backend_aot_llvm_write_guarded_call_text(context->file,
                                             context->tempCounter,
                                             "ZrLibrary_AotRuntime_Add",
                                             argsBuffer,
                                             instruction->nextLabel,
                                             context->failLabel);
    return ZR_TRUE;
}

static const TZrChar *backend_aot_llvm_binary_value_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            return "ZrLibrary_AotRuntime_LogicalEqual";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            return "ZrLibrary_AotRuntime_LogicalNotEqual";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):
            return "ZrLibrary_AotRuntime_LogicalEqualBool";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):
            return "ZrLibrary_AotRuntime_LogicalNotEqualBool";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalEqualSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalNotEqualSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalEqualUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalNotEqualUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalEqualFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalNotEqualFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_STRING):
            return "ZrLibrary_AotRuntime_LogicalEqualString";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_STRING):
            return "ZrLibrary_AotRuntime_LogicalNotEqualString";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalGreaterSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalGreaterUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalGreaterFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalLessSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalLessUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalLessFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalGreaterEqualSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalGreaterEqualUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalGreaterEqualFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            return "ZrLibrary_AotRuntime_LogicalLessEqualSigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
            return "ZrLibrary_AotRuntime_LogicalLessEqualUnsigned";
        case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            return "ZrLibrary_AotRuntime_LogicalLessEqualFloat";
        case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            return "ZrLibrary_AotRuntime_LogicalAnd";
        case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            return "ZrLibrary_AotRuntime_LogicalOr";
        case ZR_INSTRUCTION_ENUM(ADD):
        case ZR_INSTRUCTION_ENUM(ADD_STRING):
            return "ZrLibrary_AotRuntime_Add";
        case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            return "ZrLibrary_AotRuntime_AddFloat";
        case ZR_INSTRUCTION_ENUM(SUB):
            return "ZrLibrary_AotRuntime_Sub";
        case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            return "ZrLibrary_AotRuntime_SubFloat";
        case ZR_INSTRUCTION_ENUM(MUL):
            return "ZrLibrary_AotRuntime_Mul";
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddInt";
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddIntConst";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddSigned";
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddSignedConst";
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddUnsigned";
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_AddUnsignedConst";
        case ZR_INSTRUCTION_ENUM(SUB_INT):
        case ZR_INSTRUCTION_ENUM(SUB_INT_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubInt";
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_INT_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubIntConst";
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubSigned";
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_SIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubSignedConst";
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubUnsigned";
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_SubUnsignedConst";
        case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            return "ZrLibrary_AotRuntime_BitwiseAnd";
        case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            return "ZrLibrary_AotRuntime_BitwiseOr";
        case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            return "ZrLibrary_AotRuntime_BitwiseXor";
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            return "ZrLibrary_AotRuntime_BitwiseShiftLeft";
        case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            return "ZrLibrary_AotRuntime_BitwiseShiftRight";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_MulSigned";
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_SIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_MulSignedConst";
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            return "ZrLibrary_AotRuntime_MulUnsigned";
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_MulUnsigned";
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_MulUnsignedConst";
        case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            return "ZrLibrary_AotRuntime_MulFloat";
        case ZR_INSTRUCTION_ENUM(DIV):
            return "ZrLibrary_AotRuntime_Div";
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            return "ZrLibrary_AotRuntime_DivSigned";
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_SIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_DivSignedConst";
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            return "ZrLibrary_AotRuntime_DivUnsigned";
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_DivUnsignedConst";
        case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            return "ZrLibrary_AotRuntime_DivFloat";
        case ZR_INSTRUCTION_ENUM(MOD):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            return "ZrLibrary_AotRuntime_Mod";
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_SIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_ModSignedConst";
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            return "ZrLibrary_AotRuntime_ModUnsigned";
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED_CONST_PLAIN_DEST):
            return "ZrLibrary_AotRuntime_ModUnsignedConst";
        case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            return "ZrLibrary_AotRuntime_ModFloat";
        case ZR_INSTRUCTION_ENUM(POW):
            return "ZrLibrary_AotRuntime_Pow";
        case ZR_INSTRUCTION_ENUM(POW_SIGNED):
            return "ZrLibrary_AotRuntime_PowSigned";
        case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
            return "ZrLibrary_AotRuntime_PowUnsigned";
        case ZR_INSTRUCTION_ENUM(POW_FLOAT):
            return "ZrLibrary_AotRuntime_PowFloat";
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
            return "ZrLibrary_AotRuntime_ShiftLeft";
        case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
            return "ZrLibrary_AotRuntime_ShiftLeftInt";
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
            return "ZrLibrary_AotRuntime_ShiftRight";
        case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
            return "ZrLibrary_AotRuntime_ShiftRightInt";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *backend_aot_llvm_unary_value_helper_name(TZrUInt32 opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(NEG):
            return "ZrLibrary_AotRuntime_Neg";
        case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            return "ZrLibrary_AotRuntime_LogicalNot";
        case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
            return "ZrLibrary_AotRuntime_BitwiseNot";
        case ZR_INSTRUCTION_ENUM(TO_INT):
            return "ZrLibrary_AotRuntime_ToInt";
        case ZR_INSTRUCTION_ENUM(TO_STRING):
            return "ZrLibrary_AotRuntime_ToString";
        default:
            return ZR_NULL;
    }
}

TZrBool backend_aot_llvm_lower_arithmetic_value_family(const SZrAotLlvmLoweringContext *context,
                                                       const SZrAotLlvmInstructionContext *instruction) {
    const TZrChar *helperName;

    if (context == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (instruction->opcode) {
        case ZR_INSTRUCTION_ENUM(ADD_INT):
        case ZR_INSTRUCTION_ENUM(ADD_INT_PLAIN_DEST):
            return backend_aot_llvm_lower_add_int_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_PLAIN_DEST):
            return backend_aot_llvm_lower_add_signed_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST):
            return backend_aot_llvm_lower_add_unsigned_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_INT_CONST_PLAIN_DEST):
            return backend_aot_llvm_lower_add_int_const_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_SIGNED_CONST_PLAIN_DEST):
            return backend_aot_llvm_lower_add_signed_const_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST):
        case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_CONST_PLAIN_DEST):
            return backend_aot_llvm_lower_add_unsigned_const_instruction(context, instruction);
        case ZR_INSTRUCTION_ENUM(ADD):
            return backend_aot_llvm_lower_add_instruction(context, instruction);
        default:
            break;
    }

    helperName = backend_aot_llvm_binary_value_helper_name(instruction->opcode);
    if (helperName != ZR_NULL) {
        return backend_aot_llvm_lower_binary_value_instruction(context, instruction, helperName);
    }

    helperName = backend_aot_llvm_unary_value_helper_name(instruction->opcode);
    if (helperName != ZR_NULL) {
        return backend_aot_llvm_lower_unary_value_instruction(context, instruction, helperName);
    }

    return ZR_FALSE;
}
