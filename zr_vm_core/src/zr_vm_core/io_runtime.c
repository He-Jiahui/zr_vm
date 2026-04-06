//
// Runtime conversion helpers for .zro IO sources.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/runtime_decorator.h"

static TZrBool io_runtime_convert_constant(SZrState *state,
                                           const SZrIoFunctionConstantVariable *source,
                                           SZrTypeValue *destination);

static void io_runtime_init_inline_function(SZrState *state, SZrFunction *function) {
    ZrCore_Memory_RawSet(function, 0, sizeof(*function));
    ZrCore_RawObject_Construct(&function->super, ZR_RAW_OBJECT_TYPE_FUNCTION);
    /*
     * Inline child functions live inside their parent function allocation
     * instead of the GC object list, but they still appear as function values
     * in constant tables after binary import rebinding. Give them the current
     * GC generation so liveness checks treat them the same way as source-built
     * inline children copied from managed function objects.
     */
    if (state != ZR_NULL) {
        ZrCore_RawObject_MarkAsInit(state, &function->super);
    }
}

FZrNativeFunction ZrCore_Io_GetSerializableNativeHelperFunction(TZrUInt64 helperId) {
    switch ((EZrIoNativeHelperId) helperId) {
        case ZR_IO_NATIVE_HELPER_MODULE_IMPORT:
            return ZrCore_Module_ImportNativeEntry;

        case ZR_IO_NATIVE_HELPER_OWNERSHIP_UNIQUE:
            return ZrCore_Ownership_NativeUnique;

        case ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED:
            return ZrCore_Ownership_NativeShared;

        case ZR_IO_NATIVE_HELPER_OWNERSHIP_WEAK:
            return ZrCore_Ownership_NativeWeak;

        case ZR_IO_NATIVE_HELPER_OWNERSHIP_USING:
            return ZrCore_Ownership_NativeUsing;

        case ZR_IO_NATIVE_HELPER_REFLECTION_TYPEOF:
            return ZrCore_Reflection_TypeOfNativeEntry;

        case ZR_IO_NATIVE_HELPER_RUNTIME_DECORATOR_APPLY:
            return ZrCore_RuntimeDecorator_ApplyNativeEntry;

        case ZR_IO_NATIVE_HELPER_RUNTIME_MEMBER_DECORATOR_APPLY:
            return ZrCore_RuntimeDecorator_ApplyMemberNativeEntry;

        case ZR_IO_NATIVE_HELPER_NONE:
        default:
            return ZR_NULL;
    }
}

static void io_runtime_copy_typed_type_ref(SZrFunctionTypedTypeRef *destination,
                                           const SZrIoFunctionTypedTypeRef *source) {
    if (destination == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(destination, 0, sizeof(*destination));
    destination->baseType = ZR_VALUE_TYPE_OBJECT;
    destination->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    if (source == ZR_NULL) {
        return;
    }

    destination->baseType = source->baseType;
    destination->isNullable = source->isNullable;
    destination->ownershipQualifier = source->ownershipQualifier;
    destination->isArray = source->isArray;
    destination->typeName = source->typeName;
    destination->elementBaseType = source->elementBaseType;
    destination->elementTypeName = source->elementTypeName;
}

static TZrBool io_runtime_copy_semir_metadata(SZrState *state,
                                              const SZrIoFunction *source,
                                              SZrFunction *function) {
    SZrGlobalState *global;

    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    if (source->semIrTypeTableLength > 0 && source->semIrTypeTable != ZR_NULL) {
        function->semIrTypeTable = (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrFunctionTypedTypeRef) * source->semIrTypeTableLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrTypeTable == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->semIrTypeTable,
                             0,
                             sizeof(SZrFunctionTypedTypeRef) * source->semIrTypeTableLength);
        for (TZrSize index = 0; index < source->semIrTypeTableLength; index++) {
            io_runtime_copy_typed_type_ref(&function->semIrTypeTable[index], &source->semIrTypeTable[index]);
        }
        function->semIrTypeTableLength = (TZrUInt32)source->semIrTypeTableLength;
    }

    if (source->semIrOwnershipTableLength > 0 && source->semIrOwnershipTable != ZR_NULL) {
        TZrSize bytes = sizeof(SZrSemIrOwnershipEntry) * source->semIrOwnershipTableLength;
        function->semIrOwnershipTable = (SZrSemIrOwnershipEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                bytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrOwnershipTable == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->semIrOwnershipTable, source->semIrOwnershipTable, bytes);
        function->semIrOwnershipTableLength = (TZrUInt32)source->semIrOwnershipTableLength;
    }

    if (source->semIrEffectTableLength > 0 && source->semIrEffectTable != ZR_NULL) {
        TZrSize bytes = sizeof(SZrSemIrEffectEntry) * source->semIrEffectTableLength;
        function->semIrEffectTable = (SZrSemIrEffectEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                bytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrEffectTable == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->semIrEffectTable, source->semIrEffectTable, bytes);
        function->semIrEffectTableLength = (TZrUInt32)source->semIrEffectTableLength;
    }

    if (source->semIrBlockTableLength > 0 && source->semIrBlockTable != ZR_NULL) {
        TZrSize bytes = sizeof(SZrSemIrBlockEntry) * source->semIrBlockTableLength;
        function->semIrBlockTable = (SZrSemIrBlockEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                bytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrBlockTable == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->semIrBlockTable, source->semIrBlockTable, bytes);
        function->semIrBlockTableLength = (TZrUInt32)source->semIrBlockTableLength;
    }

    if (source->semIrInstructionLength > 0 && source->semIrInstructions != ZR_NULL) {
        TZrSize bytes = sizeof(SZrSemIrInstruction) * source->semIrInstructionLength;
        function->semIrInstructions = (SZrSemIrInstruction *)ZrCore_Memory_RawMallocWithType(
                global,
                bytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrInstructions == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->semIrInstructions, source->semIrInstructions, bytes);
        function->semIrInstructionLength = (TZrUInt32)source->semIrInstructionLength;
    }

    if (source->semIrDeoptTableLength > 0 && source->semIrDeoptTable != ZR_NULL) {
        TZrSize bytes = sizeof(SZrSemIrDeoptEntry) * source->semIrDeoptTableLength;
        function->semIrDeoptTable = (SZrSemIrDeoptEntry *)ZrCore_Memory_RawMallocWithType(
                global,
                bytes,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->semIrDeoptTable == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->semIrDeoptTable, source->semIrDeoptTable, bytes);
        function->semIrDeoptTableLength = (TZrUInt32)source->semIrDeoptTableLength;
    }

    return ZR_TRUE;
}

static TZrBool io_runtime_copy_callsite_cache_metadata(SZrState *state,
                                                       const SZrIoFunction *source,
                                                       SZrFunction *function) {
    SZrGlobalState *global;
    TZrSize bytes;

    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (source->callSiteCacheLength == 0 || source->callSiteCaches == ZR_NULL) {
        return ZR_TRUE;
    }

    global = state->global;
    bytes = sizeof(SZrFunctionCallSiteCacheEntry) * source->callSiteCacheLength;
    function->callSiteCaches = (SZrFunctionCallSiteCacheEntry *)ZrCore_Memory_RawMallocWithType(
            global,
            bytes,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->callSiteCaches == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(function->callSiteCaches, 0, bytes);
    for (TZrSize index = 0; index < source->callSiteCacheLength; index++) {
        const SZrIoFunctionCallSiteCacheEntry *src = &source->callSiteCaches[index];
        SZrFunctionCallSiteCacheEntry *dst = &function->callSiteCaches[index];

        dst->kind = src->kind;
        dst->instructionIndex = src->instructionIndex;
        dst->memberEntryIndex = src->memberEntryIndex;
        dst->deoptId = src->deoptId;
        dst->argumentCount = src->argumentCount;
    }
    function->callSiteCacheLength = (TZrUInt32)source->callSiteCacheLength;
    return ZR_TRUE;
}

static TZrBool io_runtime_copy_debug_infos(SZrState *state,
                                           const SZrIoFunction *source,
                                           SZrFunction *function) {
    const SZrIoFunctionDebugInfo *debugInfo;
    SZrGlobalState *global;
    TZrSize debugInstructionCount;
    SZrIoInstructionSourceRange previousRange;
    TZrUInt32 previousLine = 0;
    TZrBool hasPreviousLine = ZR_FALSE;

    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL || state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 0 || source->debugInfosLength == 0 || source->debugInfos == ZR_NULL) {
        return ZR_TRUE;
    }

    debugInfo = &source->debugInfos[0];
    ZrCore_Memory_RawSet(&previousRange, 0, sizeof(previousRange));
    if (debugInfo->instructionsLength == 0 || debugInfo->instructionsLine == ZR_NULL) {
        function->sourceCodeList = debugInfo->sourceFile;
        function->sourceHash = debugInfo->sourceHash;
        return ZR_TRUE;
    }

    function->sourceCodeList = debugInfo->sourceFile;
    function->sourceHash = debugInfo->sourceHash;

    global = state->global;
    debugInstructionCount = debugInfo->instructionsLength;
    if (debugInstructionCount > function->instructionsLength) {
        debugInstructionCount = function->instructionsLength;
    }

    function->lineInSourceList = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                               sizeof(TZrUInt32) * function->instructionsLength,
                                                                               ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->lineInSourceList == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(function->lineInSourceList, 0, sizeof(TZrUInt32) * function->instructionsLength);

    for (TZrUInt32 index = 0; index < (TZrUInt32)debugInstructionCount; index++) {
        TZrUInt32 currentLine = (TZrUInt32)debugInfo->instructionsLine[index];
        function->lineInSourceList[index] = currentLine;
        previousLine = currentLine;
        if (debugInfo->instructionRanges != ZR_NULL) {
            previousRange = debugInfo->instructionRanges[index];
        } else {
            previousRange.startLine = currentLine;
            previousRange.endLine = currentLine;
            previousRange.startColumn = 0;
            previousRange.endColumn = 0;
        }
        hasPreviousLine = ZR_TRUE;
    }

    if (hasPreviousLine) {
        for (TZrUInt32 index = (TZrUInt32)debugInstructionCount; index < function->instructionsLength; index++) {
            function->lineInSourceList[index] = previousLine;
        }
    }

    function->executionLocationInfoList = (SZrFunctionExecutionLocationInfo *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionExecutionLocationInfo) * function->instructionsLength,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->executionLocationInfoList == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(function->executionLocationInfoList,
                         0,
                         sizeof(SZrFunctionExecutionLocationInfo) * function->instructionsLength);

    for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
        function->executionLocationInfoList[index].currentInstructionOffset = index;
        function->executionLocationInfoList[index].lineInSource = function->lineInSourceList[index];
        if ((TZrSize)index < debugInstructionCount && debugInfo->instructionRanges != ZR_NULL) {
            function->executionLocationInfoList[index].columnInSourceStart =
                    debugInfo->instructionRanges[index].startColumn;
            function->executionLocationInfoList[index].lineInSourceEnd =
                    debugInfo->instructionRanges[index].endLine != 0
                            ? debugInfo->instructionRanges[index].endLine
                            : function->lineInSourceList[index];
            function->executionLocationInfoList[index].columnInSourceEnd = debugInfo->instructionRanges[index].endColumn;
        } else if (hasPreviousLine) {
            function->executionLocationInfoList[index].columnInSourceStart = previousRange.startColumn;
            function->executionLocationInfoList[index].lineInSourceEnd =
                    previousRange.endLine != 0 ? previousRange.endLine : function->lineInSourceList[index];
            function->executionLocationInfoList[index].columnInSourceEnd = previousRange.endColumn;
        } else {
            function->executionLocationInfoList[index].lineInSourceEnd = function->lineInSourceList[index];
        }
    }
    function->executionLocationInfoLength = function->instructionsLength;

    return ZR_TRUE;
}

static TZrBool io_runtime_copy_metadata_parameters(SZrState *state,
                                                   SZrFunctionMetadataParameter **outParameters,
                                                   TZrUInt32 *outCount,
                                                   const SZrIoFunctionMetadataParameter *sourceParameters,
                                                   TZrSize sourceCount) {
    SZrGlobalState *global;

    if (outParameters == ZR_NULL || outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    *outParameters = ZR_NULL;
    *outCount = 0;
    if (state == ZR_NULL || sourceParameters == ZR_NULL || sourceCount == 0) {
        return ZR_TRUE;
    }

    global = state->global;
    *outParameters = (SZrFunctionMetadataParameter *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionMetadataParameter) * sourceCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (*outParameters == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(*outParameters, 0, sizeof(SZrFunctionMetadataParameter) * sourceCount);
    for (TZrSize index = 0; index < sourceCount; index++) {
        (*outParameters)[index].name = sourceParameters[index].name;
        io_runtime_copy_typed_type_ref(&(*outParameters)[index].type, &sourceParameters[index].type);
        (*outParameters)[index].hasDefaultValue = sourceParameters[index].hasDefaultValue ? ZR_TRUE : ZR_FALSE;
        if ((*outParameters)[index].hasDefaultValue &&
            !io_runtime_convert_constant(state,
                                         &sourceParameters[index].defaultValue,
                                         &(*outParameters)[index].defaultValue)) {
            return ZR_FALSE;
        }
        (*outParameters)[index].hasDecoratorMetadata = sourceParameters[index].hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
        if ((*outParameters)[index].hasDecoratorMetadata &&
            !io_runtime_convert_constant(state,
                                         &sourceParameters[index].decoratorMetadataValue,
                                         &(*outParameters)[index].decoratorMetadataValue)) {
            return ZR_FALSE;
        }
        if (sourceParameters[index].decoratorNamesLength > 0) {
            (*outParameters)[index].decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
                    global,
                    sizeof(SZrString *) * sourceParameters[index].decoratorNamesLength,
                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            if ((*outParameters)[index].decoratorNames == ZR_NULL) {
                return ZR_FALSE;
            }

            ZrCore_Memory_RawSet((*outParameters)[index].decoratorNames,
                                 0,
                                 sizeof(SZrString *) * sourceParameters[index].decoratorNamesLength);
            for (TZrSize decoratorIndex = 0;
                 decoratorIndex < sourceParameters[index].decoratorNamesLength;
                 decoratorIndex++) {
                (*outParameters)[index].decoratorNames[decoratorIndex] =
                        sourceParameters[index].decoratorNames[decoratorIndex];
            }
            (*outParameters)[index].decoratorCount = (TZrUInt32)sourceParameters[index].decoratorNamesLength;
        }
    }

    *outCount = (TZrUInt32)sourceCount;
    return ZR_TRUE;
}

static TZrBool io_runtime_populate_function(SZrState *state,
                                            const SZrIoFunction *source,
                                            SZrFunction *function);

static TZrBool io_runtime_convert_constant(SZrState *state,
                                           const SZrIoFunctionConstantVariable *source,
                                           SZrTypeValue *destination) {
    FZrNativeFunction nativeHelper;

    if (state == ZR_NULL || source == ZR_NULL || destination == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(destination);
    nativeHelper = ZrCore_Io_GetSerializableNativeHelperFunction(source->startLine);

    switch (source->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_TRUE;

        case ZR_VALUE_TYPE_BOOL:
            destination->type = ZR_VALUE_TYPE_BOOL;
            destination->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            destination->type = source->type;
            destination->value.nativeObject.nativeInt64 = source->value.nativeObject.nativeInt64;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            destination->type = source->type;
            destination->value.nativeObject.nativeUInt64 = source->value.nativeObject.nativeUInt64;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            destination->type = source->type;
            destination->value.nativeObject.nativeDouble = source->value.nativeObject.nativeDouble;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_STRING:
            if (source->value.object == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(state, destination, source->value.object);
            destination->type = ZR_VALUE_TYPE_STRING;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            if (source->value.object == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(state, destination, source->value.object);
            destination->type = source->type;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            if (nativeHelper == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_ResetAsNull(destination);
            destination->type = ZR_VALUE_TYPE_NATIVE_POINTER;
            destination->value.nativeFunction = nativeHelper;
            destination->isGarbageCollectable = ZR_FALSE;
            destination->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            SZrFunction *functionValue;

            if (!source->hasFunctionValue || source->functionValue == ZR_NULL) {
                if (source->type == ZR_VALUE_TYPE_CLOSURE && nativeHelper != ZR_NULL) {
                    SZrClosureNative *closure = ZrCore_ClosureNative_New(state, 0);
                    if (closure == ZR_NULL) {
                        return ZR_FALSE;
                    }

                    closure->nativeFunction = nativeHelper;
                    ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
                    destination->type = ZR_VALUE_TYPE_CLOSURE;
                    return ZR_TRUE;
                }

                return ZR_FALSE;
            }

            functionValue = ZrCore_Function_New(state);
            if (functionValue == ZR_NULL) {
                return ZR_FALSE;
            }

            if (!io_runtime_populate_function(state, source->functionValue, functionValue)) {
                ZrCore_Function_Free(state, functionValue);
                return ZR_FALSE;
            }

            if (source->type == ZR_VALUE_TYPE_CLOSURE) {
                SZrClosure *closure = ZrCore_Closure_New(state, 0);
                if (closure == ZR_NULL) {
                    ZrCore_Function_Free(state, functionValue);
                    return ZR_FALSE;
                }
                closure->function = functionValue;
                ZrCore_Closure_InitValue(state, closure);
                ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
                destination->type = ZR_VALUE_TYPE_CLOSURE;
            } else {
                ZrCore_Value_InitAsRawObject(state, destination, ZR_CAST_RAW_OBJECT_AS_SUPER(functionValue));
                destination->type = ZR_VALUE_TYPE_FUNCTION;
            }
            return ZR_TRUE;
        }

        default:
            return ZR_FALSE;
    }
}

static TZrBool io_runtime_populate_function(SZrState *state,
                                            const SZrIoFunction *source,
                                            SZrFunction *function) {
    SZrGlobalState *global;

    if (state == ZR_NULL || source == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    global = state->global;
    function->functionName = source->name;
    function->parameterCount = (TZrUInt16)source->parametersLength;
    function->hasVariableArguments = source->hasVarArgs ? ZR_TRUE : ZR_FALSE;
    function->stackSize = source->stackSize;
    function->lineInSourceStart = (TZrUInt32)source->startLine;
    function->lineInSourceEnd = (TZrUInt32)source->endLine;

    if (source->instructionsLength > 0) {
        TZrSize instructionBytes = sizeof(TZrInstruction) * source->instructionsLength;
        function->instructionsList =
                (TZrInstruction *)ZrCore_Memory_RawMallocWithType(global, instructionBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->instructionsList == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawCopy(function->instructionsList, source->instructions, instructionBytes);
        function->instructionsLength = (TZrUInt32)source->instructionsLength;
    }

    if (!io_runtime_copy_debug_infos(state, source, function)) {
        return ZR_FALSE;
    }

    if (source->localVariablesLength > 0) {
        TZrSize localBytes = sizeof(SZrFunctionLocalVariable) * source->localVariablesLength;
        function->localVariableList =
                (SZrFunctionLocalVariable *)ZrCore_Memory_RawMallocWithType(global, localBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->localVariableList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->localVariablesLength; index++) {
            function->localVariableList[index].name = ZR_NULL;
            function->localVariableList[index].stackSlot = (TZrUInt32)index;
            function->localVariableList[index].offsetActivate =
                    (TZrMemoryOffset)source->localVariables[index].instructionStartIndex;
            function->localVariableList[index].offsetDead =
                    (TZrMemoryOffset)source->localVariables[index].instructionEndIndex;
        }
        function->localVariableLength = (TZrUInt32)source->localVariablesLength;
    }

    if (source->closureVariablesLength > 0) {
        TZrSize closureBytes = sizeof(SZrFunctionClosureVariable) * source->closureVariablesLength;
        function->closureValueList =
                (SZrFunctionClosureVariable *)ZrCore_Memory_RawMallocWithType(global,
                                                                              closureBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->closureValueList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->closureVariablesLength; index++) {
            function->closureValueList[index].name = source->closureVariables[index].name;
            function->closureValueList[index].inStack = source->closureVariables[index].inStack ? ZR_TRUE : ZR_FALSE;
            function->closureValueList[index].index = source->closureVariables[index].index;
            function->closureValueList[index].valueType = (EZrValueType)source->closureVariables[index].valueType;
        }
        function->closureValueLength = (TZrUInt32)source->closureVariablesLength;
    }

    if (source->catchClauseCount > 0) {
        TZrSize catchBytes = sizeof(SZrFunctionCatchClauseInfo) * source->catchClauseCount;
        function->catchClauseList =
                (SZrFunctionCatchClauseInfo *)ZrCore_Memory_RawMallocWithType(global,
                                                                              catchBytes,
                                                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->catchClauseList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->catchClauseCount; index++) {
            function->catchClauseList[index].typeName = source->catchClauses[index].typeName;
            function->catchClauseList[index].targetInstructionOffset =
                    (TZrMemoryOffset)source->catchClauses[index].targetInstructionOffset;
        }
        function->catchClauseCount = (TZrUInt32)source->catchClauseCount;
    }

    if (source->exceptionHandlerCount > 0) {
        TZrSize handlerBytes = sizeof(SZrFunctionExceptionHandlerInfo) * source->exceptionHandlerCount;
        function->exceptionHandlerList =
                (SZrFunctionExceptionHandlerInfo *)ZrCore_Memory_RawMallocWithType(global,
                                                                                   handlerBytes,
                                                                                   ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exceptionHandlerList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->exceptionHandlerCount; index++) {
            function->exceptionHandlerList[index].protectedStartInstructionOffset =
                    (TZrMemoryOffset)source->exceptionHandlers[index].protectedStartInstructionOffset;
            function->exceptionHandlerList[index].finallyTargetInstructionOffset =
                    (TZrMemoryOffset)source->exceptionHandlers[index].finallyTargetInstructionOffset;
            function->exceptionHandlerList[index].afterFinallyInstructionOffset =
                    (TZrMemoryOffset)source->exceptionHandlers[index].afterFinallyInstructionOffset;
            function->exceptionHandlerList[index].catchClauseStartIndex =
                    source->exceptionHandlers[index].catchClauseStartIndex;
            function->exceptionHandlerList[index].catchClauseCount =
                    source->exceptionHandlers[index].catchClauseCount;
            function->exceptionHandlerList[index].hasFinally =
                    source->exceptionHandlers[index].hasFinally ? ZR_TRUE : ZR_FALSE;
        }
        function->exceptionHandlerCount = (TZrUInt32)source->exceptionHandlerCount;
    }

    if (source->constantVariablesLength > 0) {
        TZrSize constantBytes = sizeof(SZrTypeValue) * source->constantVariablesLength;
        function->constantValueList =
                (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(global, constantBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->constantValueList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->constantVariablesLength; index++) {
            if (!io_runtime_convert_constant(state,
                                             &source->constantVariables[index],
                                             &function->constantValueList[index])) {
                return ZR_FALSE;
            }
        }
        function->constantValueLength = (TZrUInt32)source->constantVariablesLength;
    }

    if (source->exportedVariablesLength > 0) {
        TZrSize exportBytes = sizeof(*function->exportedVariables) * source->exportedVariablesLength;
        function->exportedVariables =
                (struct SZrFunctionExportedVariable *)ZrCore_Memory_RawMallocWithType(global,
                                                                                      exportBytes,
                                                                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->exportedVariables == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->exportedVariablesLength; index++) {
            function->exportedVariables[index].name = source->exportedVariables[index].name;
            function->exportedVariables[index].stackSlot = source->exportedVariables[index].stackSlot;
            function->exportedVariables[index].accessModifier = source->exportedVariables[index].accessModifier;
        }
        function->exportedVariableLength = (TZrUInt32)source->exportedVariablesLength;
    }

    if (source->typedLocalBindingsLength > 0) {
        TZrSize bindingBytes = sizeof(SZrFunctionTypedLocalBinding) * source->typedLocalBindingsLength;
        function->typedLocalBindings =
                (SZrFunctionTypedLocalBinding *)ZrCore_Memory_RawMallocWithType(global,
                                                                                bindingBytes,
                                                                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->typedLocalBindings == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->typedLocalBindingsLength; index++) {
            function->typedLocalBindings[index].name = source->typedLocalBindings[index].name;
            function->typedLocalBindings[index].stackSlot = source->typedLocalBindings[index].stackSlot;
            io_runtime_copy_typed_type_ref(&function->typedLocalBindings[index].type,
                                           &source->typedLocalBindings[index].type);
        }
        function->typedLocalBindingLength = (TZrUInt32)source->typedLocalBindingsLength;
    }

    if (source->typedExportedSymbolsLength > 0) {
        TZrSize exportSymbolBytes = sizeof(SZrFunctionTypedExportSymbol) * source->typedExportedSymbolsLength;
        function->typedExportedSymbols =
                (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(global,
                                                                                exportSymbolBytes,
                                                                                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->typedExportedSymbols == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->typedExportedSymbolsLength; index++) {
            const SZrIoFunctionTypedExportSymbol *sourceSymbol = &source->typedExportedSymbols[index];
            SZrFunctionTypedExportSymbol *destinationSymbol = &function->typedExportedSymbols[index];

            ZrCore_Memory_RawSet(destinationSymbol, 0, sizeof(*destinationSymbol));
            destinationSymbol->name = sourceSymbol->name;
            destinationSymbol->stackSlot = sourceSymbol->stackSlot;
            destinationSymbol->accessModifier = sourceSymbol->accessModifier;
            destinationSymbol->symbolKind = sourceSymbol->symbolKind;
            io_runtime_copy_typed_type_ref(&destinationSymbol->valueType, &sourceSymbol->valueType);
            destinationSymbol->parameterCount = (TZrUInt32)sourceSymbol->parameterCount;
            destinationSymbol->lineInSourceStart = sourceSymbol->lineInSourceStart;
            destinationSymbol->columnInSourceStart = sourceSymbol->columnInSourceStart;
            destinationSymbol->lineInSourceEnd = sourceSymbol->lineInSourceEnd;
            destinationSymbol->columnInSourceEnd = sourceSymbol->columnInSourceEnd;

            if (sourceSymbol->parameterCount > 0) {
                destinationSymbol->parameterTypes =
                        (SZrFunctionTypedTypeRef *)ZrCore_Memory_RawMallocWithType(global,
                                                                                   sizeof(SZrFunctionTypedTypeRef) *
                                                                                           sourceSymbol->parameterCount,
                                                                                   ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                if (destinationSymbol->parameterTypes == ZR_NULL) {
                    return ZR_FALSE;
                }

                for (TZrSize paramIndex = 0; paramIndex < sourceSymbol->parameterCount; paramIndex++) {
                    io_runtime_copy_typed_type_ref(&destinationSymbol->parameterTypes[paramIndex],
                                                   &sourceSymbol->parameterTypes[paramIndex]);
                }
            }
        }
        function->typedExportedSymbolLength = (TZrUInt32)source->typedExportedSymbolsLength;
    }

    if (!io_runtime_copy_metadata_parameters(state,
                                             &function->parameterMetadata,
                                             &function->parameterMetadataCount,
                                             source->parameterMetadata,
                                             source->parameterMetadataLength)) {
        return ZR_FALSE;
    }

    if (source->compileTimeVariableInfosLength > 0) {
        TZrSize infoBytes = sizeof(SZrFunctionCompileTimeVariableInfo) * source->compileTimeVariableInfosLength;
        function->compileTimeVariableInfos =
                (SZrFunctionCompileTimeVariableInfo *)ZrCore_Memory_RawMallocWithType(global,
                                                                                      infoBytes,
                                                                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->compileTimeVariableInfos == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->compileTimeVariableInfos, 0, infoBytes);
        for (TZrSize index = 0; index < source->compileTimeVariableInfosLength; index++) {
            SZrFunctionCompileTimeVariableInfo *destinationInfo = &function->compileTimeVariableInfos[index];
            const SZrIoFunctionCompileTimeVariableInfo *sourceInfo = &source->compileTimeVariableInfos[index];

            destinationInfo->name = sourceInfo->name;
            destinationInfo->lineInSourceStart = sourceInfo->lineInSourceStart;
            destinationInfo->lineInSourceEnd = sourceInfo->lineInSourceEnd;
            io_runtime_copy_typed_type_ref(&destinationInfo->type, &sourceInfo->type);
            destinationInfo->pathBindings = ZR_NULL;
            destinationInfo->pathBindingCount = 0;
            if (sourceInfo->pathBindingsLength > 0) {
                destinationInfo->pathBindings = (SZrFunctionCompileTimePathBinding *)ZrCore_Memory_RawMallocWithType(
                        global,
                        sizeof(SZrFunctionCompileTimePathBinding) * sourceInfo->pathBindingsLength,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
                if (destinationInfo->pathBindings == ZR_NULL) {
                    return ZR_FALSE;
                }
                ZrCore_Memory_RawSet(destinationInfo->pathBindings,
                                     0,
                                     sizeof(SZrFunctionCompileTimePathBinding) * sourceInfo->pathBindingsLength);
                for (TZrSize bindingIndex = 0; bindingIndex < sourceInfo->pathBindingsLength; bindingIndex++) {
                    destinationInfo->pathBindings[bindingIndex].path = sourceInfo->pathBindings[bindingIndex].path;
                    destinationInfo->pathBindings[bindingIndex].targetKind =
                            sourceInfo->pathBindings[bindingIndex].targetKind;
                    destinationInfo->pathBindings[bindingIndex].targetName =
                            sourceInfo->pathBindings[bindingIndex].targetName;
                }
                destinationInfo->pathBindingCount = (TZrUInt32)sourceInfo->pathBindingsLength;
            }
        }
        function->compileTimeVariableInfoLength = (TZrUInt32)source->compileTimeVariableInfosLength;
    }

    if (source->compileTimeFunctionInfosLength > 0) {
        TZrSize infoBytes = sizeof(SZrFunctionCompileTimeFunctionInfo) * source->compileTimeFunctionInfosLength;
        function->compileTimeFunctionInfos =
                (SZrFunctionCompileTimeFunctionInfo *)ZrCore_Memory_RawMallocWithType(global,
                                                                                      infoBytes,
                                                                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->compileTimeFunctionInfos == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->compileTimeFunctionInfos, 0, infoBytes);
        for (TZrSize index = 0; index < source->compileTimeFunctionInfosLength; index++) {
            SZrFunctionCompileTimeFunctionInfo *destinationInfo = &function->compileTimeFunctionInfos[index];
            const SZrIoFunctionCompileTimeFunctionInfo *sourceInfo = &source->compileTimeFunctionInfos[index];

            destinationInfo->name = sourceInfo->name;
            destinationInfo->lineInSourceStart = sourceInfo->lineInSourceStart;
            destinationInfo->lineInSourceEnd = sourceInfo->lineInSourceEnd;
            io_runtime_copy_typed_type_ref(&destinationInfo->returnType, &sourceInfo->returnType);
            if (!io_runtime_copy_metadata_parameters(state,
                                                     &destinationInfo->parameters,
                                                     &destinationInfo->parameterCount,
                                                     sourceInfo->parameters,
                                                     sourceInfo->parameterCount)) {
                return ZR_FALSE;
            }
        }
        function->compileTimeFunctionInfoLength = (TZrUInt32)source->compileTimeFunctionInfosLength;
    }

    if (source->testInfosLength > 0) {
        TZrSize infoBytes = sizeof(SZrFunctionTestInfo) * source->testInfosLength;
        function->testInfos = (SZrFunctionTestInfo *)ZrCore_Memory_RawMallocWithType(global,
                                                                                     infoBytes,
                                                                                     ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->testInfos == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->testInfos, 0, infoBytes);
        for (TZrSize index = 0; index < source->testInfosLength; index++) {
            SZrFunctionTestInfo *destinationInfo = &function->testInfos[index];
            const SZrIoFunctionTestInfo *sourceInfo = &source->testInfos[index];

            destinationInfo->name = sourceInfo->name;
            destinationInfo->hasVariableArguments = sourceInfo->hasVariableArguments ? ZR_TRUE : ZR_FALSE;
            destinationInfo->lineInSourceStart = sourceInfo->lineInSourceStart;
            destinationInfo->lineInSourceEnd = sourceInfo->lineInSourceEnd;
            if (!io_runtime_copy_metadata_parameters(state,
                                                     &destinationInfo->parameters,
                                                     &destinationInfo->parameterCount,
                                                     sourceInfo->parameters,
                                                     sourceInfo->parameterCount)) {
                return ZR_FALSE;
            }
        }
        function->testInfoLength = (TZrUInt32)source->testInfosLength;
    }

    if (source->hasDecoratorMetadata) {
        if (!io_runtime_convert_constant(state, &source->decoratorMetadataValue, &function->decoratorMetadataValue)) {
            return ZR_FALSE;
        }
        function->hasDecoratorMetadata = ZR_TRUE;
    }

    if (source->decoratorNamesLength > 0) {
        function->decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
                global,
                sizeof(SZrString *) * source->decoratorNamesLength,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->decoratorNames == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->decoratorNames,
                             0,
                             sizeof(SZrString *) * source->decoratorNamesLength);
        for (TZrSize index = 0; index < source->decoratorNamesLength; index++) {
            function->decoratorNames[index] = source->decoratorNames[index];
        }
        function->decoratorCount = (TZrUInt32)source->decoratorNamesLength;
    }

    if (source->memberEntriesLength > 0) {
        TZrSize entryBytes = sizeof(SZrFunctionMemberEntry) * source->memberEntriesLength;
        function->memberEntries =
                (SZrFunctionMemberEntry *)ZrCore_Memory_RawMallocWithType(global,
                                                                          entryBytes,
                                                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->memberEntries == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawSet(function->memberEntries, 0, entryBytes);
        for (TZrSize index = 0; index < source->memberEntriesLength; index++) {
            const SZrIoFunctionMemberEntry *sourceEntry = &source->memberEntries[index];
            SZrFunctionMemberEntry *destinationEntry = &function->memberEntries[index];

            destinationEntry->symbol = sourceEntry->symbol;
            destinationEntry->entryKind = sourceEntry->entryKind;
            destinationEntry->reserved0 = sourceEntry->reserved0;
            destinationEntry->reserved1 = sourceEntry->reserved1;
            destinationEntry->prototypeIndex = sourceEntry->prototypeIndex;
            destinationEntry->descriptorIndex = sourceEntry->descriptorIndex;
        }
        function->memberEntryLength = (TZrUInt32)source->memberEntriesLength;
    }

    if (!io_runtime_copy_semir_metadata(state, source, function)) {
        return ZR_FALSE;
    }
    if (!io_runtime_copy_callsite_cache_metadata(state, source, function)) {
        return ZR_FALSE;
    }

    if (source->prototypeData != ZR_NULL &&
        source->prototypeDataLength >= sizeof(TZrUInt32) &&
        source->prototypesLength > 0) {
        function->prototypeData =
                (TZrByte *)ZrCore_Memory_RawMallocWithType(global,
                                                           source->prototypeDataLength,
                                                           ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->prototypeData == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrCore_Memory_RawCopy(function->prototypeData, source->prototypeData, source->prototypeDataLength);
        function->prototypeDataLength = (TZrUInt32)source->prototypeDataLength;
        function->prototypeCount = *(const TZrUInt32 *)source->prototypeData;
        if (function->prototypeCount == 0) {
            function->prototypeCount = (TZrUInt32)source->prototypesLength;
        }
    }

    if (source->closuresLength > 0) {
        TZrSize childBytes = sizeof(SZrFunction) * source->closuresLength;
        function->childFunctionList =
                (SZrFunction *)ZrCore_Memory_RawMallocWithType(global, childBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->childFunctionList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->closuresLength; index++) {
            io_runtime_init_inline_function(state, &function->childFunctionList[index]);
            if (source->closures[index].subFunction == ZR_NULL ||
                !io_runtime_populate_function(state,
                                              source->closures[index].subFunction,
                                              &function->childFunctionList[index])) {
                return ZR_FALSE;
            }
        }
        function->childFunctionLength = (TZrUInt32)source->closuresLength;
    }

    ZrCore_Function_RebindConstantFunctionValuesToChildren(function);

    return ZR_TRUE;
}

struct SZrFunction *ZrCore_Io_LoadEntryFunctionToRuntime(struct SZrState *state, const SZrIoSource *source) {
    const SZrIoModule *module;
    SZrFunction *function;

    if (state == ZR_NULL || source == ZR_NULL || source->modules == ZR_NULL || source->modulesLength == 0) {
        return ZR_NULL;
    }

    module = &source->modules[0];
    if (module->entryFunction == ZR_NULL) {
        return ZR_NULL;
    }

    function = ZrCore_Function_New(state);
    if (function == ZR_NULL) {
        return ZR_NULL;
    }

    if (!io_runtime_populate_function(state, module->entryFunction, function)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    return function;
}
