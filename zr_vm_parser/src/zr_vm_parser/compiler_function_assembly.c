#include "compiler_internal.h"

static TZrBool compiler_constant_refs_function(const SZrTypeValue *value, SZrFunction *function) {
    if (value == ZR_NULL || function == ZR_NULL || value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->type == ZR_VALUE_TYPE_FUNCTION &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
        return ZR_CAST(SZrFunction *, value->value.object) == function;
    }

    if (value->type == ZR_VALUE_TYPE_CLOSURE &&
        !value->isNative &&
        value->value.object->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
        SZrClosure *closure = ZR_CAST(SZrClosure *, value->value.object);
        return closure != ZR_NULL && closure->function == function;
    }

    return ZR_FALSE;
}

TZrBool compiler_attach_detached_function_prototype_context(SZrState *state,
                                                                   SZrFunction *detachedFunction,
                                                                   SZrFunction *entryFunction) {
    SZrGlobalState *global;
    SZrTypeValue *newConstants;
    TZrSize oldLength;
    TZrSize newLength;
    SZrTypeValue entryFunctionValue;

    if (state == ZR_NULL || detachedFunction == ZR_NULL || entryFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    if (entryFunction->prototypeData == ZR_NULL || entryFunction->prototypeDataLength == 0 || entryFunction->prototypeCount == 0) {
        return ZR_TRUE;
    }

    for (TZrUInt32 index = 0; index < detachedFunction->constantValueLength; index++) {
        if (compiler_constant_refs_function(&detachedFunction->constantValueList[index], entryFunction)) {
            return ZR_TRUE;
        }
    }

    global = state->global;
    oldLength = detachedFunction->constantValueLength;
    newLength = oldLength + 1;
    newConstants = (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global,
                                                                    sizeof(SZrTypeValue) * newLength,
                                                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (newConstants == ZR_NULL) {
        return ZR_FALSE;
    }

    if (oldLength > 0 && detachedFunction->constantValueList != ZR_NULL) {
        memcpy(newConstants, detachedFunction->constantValueList, sizeof(SZrTypeValue) * oldLength);
        ZrCore_Memory_RawFreeWithType(global,
                                      detachedFunction->constantValueList,
                                      sizeof(SZrTypeValue) * oldLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    ZrCore_Value_InitAsRawObject(state, &entryFunctionValue, ZR_CAST_RAW_OBJECT_AS_SUPER(entryFunction));
    entryFunctionValue.type = ZR_VALUE_TYPE_FUNCTION;
    newConstants[oldLength] = entryFunctionValue;

    detachedFunction->constantValueList = newConstants;
    detachedFunction->constantValueLength = (TZrUInt32) newLength;
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_instructions(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->instructions.length == 0) {
        function->instructionsLength = 0;
        function->instructionsList = ZR_NULL;
        return ZR_TRUE;
    }

    TZrSize instructionSize = cs->instructions.length * sizeof(TZrInstruction);
    function->instructionsList =
            (TZrInstruction *) ZrCore_Memory_RawMallocWithType(global, instructionSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(function->instructionsList, cs->instructions.head, instructionSize);
    function->instructionsLength = (TZrUInt32) cs->instructions.length;
    cs->instructionCount = cs->instructions.length;
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_constants(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->constants.length == 0) {
        function->constantValueLength = 0;
        function->constantValueList = ZR_NULL;
        return ZR_TRUE;
    }

    TZrSize constantSize = cs->constants.length * sizeof(SZrTypeValue);
    function->constantValueList =
            (SZrTypeValue *) ZrCore_Memory_RawMallocWithType(global, constantSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->constantValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(function->constantValueList, cs->constants.head, constantSize);
    function->constantValueLength = (TZrUInt32) cs->constants.length;
    cs->constantCount = cs->constants.length;
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_locals(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->localVars.length == 0) {
        function->localVariableLength = 0;
        function->localVariableList = ZR_NULL;
        return ZR_TRUE;
    }

    TZrSize localVariableSize = cs->localVars.length * sizeof(SZrFunctionLocalVariable);
    function->localVariableList = (SZrFunctionLocalVariable *) ZrCore_Memory_RawMallocWithType(
            global, localVariableSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->localVariableList == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(function->localVariableList, cs->localVars.head, localVariableSize);
    function->localVariableLength = (TZrUInt32) cs->localVars.length;
    cs->localVarCount = cs->localVars.length;
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_closures(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->closureVarCount == 0) {
        function->closureValueLength = 0;
        function->closureValueList = ZR_NULL;
        return ZR_TRUE;
    }

    TZrSize closureSize = cs->closureVarCount * sizeof(SZrFunctionClosureVariable);
    function->closureValueList = (SZrFunctionClosureVariable *) ZrCore_Memory_RawMallocWithType(
            global, closureSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->closureValueList == ZR_NULL) {
        return ZR_FALSE;
    }

    memcpy(function->closureValueList, cs->closureVars.head, closureSize);
    function->closureValueLength = (TZrUInt32) cs->closureVarCount;
    return ZR_TRUE;
}

static TZrBool compiler_copy_function_children(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->childFunctions.length == 0) {
        function->childFunctionLength = 0;
        function->childFunctionList = ZR_NULL;
        return ZR_TRUE;
    }

    TZrSize childFunctionSize = cs->childFunctions.length * sizeof(SZrFunction);
    function->childFunctionList =
            (SZrFunction *) ZrCore_Memory_RawMallocWithType(global, childFunctionSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->childFunctionList == ZR_NULL) {
        return ZR_FALSE;
    }

    SZrFunction **sourceChildren = (SZrFunction **) cs->childFunctions.head;
    for (TZrSize index = 0; index < cs->childFunctions.length; index++) {
        if (sourceChildren[index] != ZR_NULL) {
            function->childFunctionList[index] = *sourceChildren[index];
        }
    }

    function->childFunctionLength = (TZrUInt32) cs->childFunctions.length;
    ZrCore_Function_RebindConstantFunctionValuesToChildren(function);
    for (TZrSize index = 0; index < cs->childFunctions.length; index++) {
        ZrCore_Function_RebindConstantFunctionValuesToChildren(&function->childFunctionList[index]);
    }
    return ZR_TRUE;
}

static TZrBool compiler_validate_function_child_graph(SZrCompilerState *cs,
                                                             SZrFunction *function,
                                                             SZrAstNode *sourceNode) {
    if (ZrCore_Function_ValidateCreateClosureTargetsInChildGraph(function)) {
        return ZR_TRUE;
    }

    ZrParser_Compiler_Error(cs,
                            "Compiled function emitted CREATE_CLOSURE target outside childFunctions graph",
                            sourceNode->location);
    return ZR_FALSE;
}

static TZrBool compiler_copy_function_exports(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global = cs->state->global;

    if (cs->proVariables.length == 0) {
        function->exportedVariables = ZR_NULL;
        function->exportedVariableLength = 0;
        return ZR_TRUE;
    }

    TZrSize exportVariableSize = cs->proVariables.length * sizeof(struct SZrFunctionExportedVariable);
    function->exportedVariables = (struct SZrFunctionExportedVariable *) ZrCore_Memory_RawMallocWithType(
            global, exportVariableSize, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->exportedVariables == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < cs->proVariables.length; index++) {
        SZrExportedVariable *sourceVariable = (SZrExportedVariable *) ZrCore_Array_Get(&cs->proVariables, index);
        if (sourceVariable != ZR_NULL) {
            function->exportedVariables[index].name = sourceVariable->name;
            function->exportedVariables[index].stackSlot = sourceVariable->stackSlot;
            function->exportedVariables[index].accessModifier = (TZrUInt8) sourceVariable->accessModifier;
            function->exportedVariables[index].exportKind = (TZrUInt8)sourceVariable->exportKind;
            function->exportedVariables[index].readiness = (TZrUInt8)sourceVariable->readiness;
            function->exportedVariables[index].reserved0 = 0;
            function->exportedVariables[index].callableChildIndex = sourceVariable->callableChildIndex;
        }
    }

    function->exportedVariableLength = (TZrUInt32) cs->proVariables.length;
    return ZR_TRUE;
}

static void compiler_finalize_optional_function_lengths(SZrFunction *function) {
    if (function->instructionsList == ZR_NULL) {
        function->instructionsLength = 0;
    }
    if (function->constantValueList == ZR_NULL) {
        function->constantValueLength = 0;
    }
    if (function->localVariableList == ZR_NULL) {
        function->localVariableLength = 0;
    }
    if (function->closureValueList == ZR_NULL) {
        function->closureValueLength = 0;
    }
    if (function->childFunctionList == ZR_NULL) {
        function->childFunctionLength = 0;
    }
    if (function->executionLocationInfoList == ZR_NULL) {
        function->executionLocationInfoLength = 0;
    }
    if (function->exportedVariables == ZR_NULL) {
        function->exportedVariableLength = 0;
    }
}

TZrBool compiler_assemble_final_function(SZrCompilerState *cs,
                                                SZrFunction *function,
                                                SZrAstNode *sourceNode,
                                                TZrBool copyCurrentFunctionBuffers,
                                                TZrBool preserveExistingSignature) {
    if (cs == ZR_NULL || function == ZR_NULL || sourceNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (copyCurrentFunctionBuffers) {
        if (!compiler_copy_function_instructions(cs, function) ||
            !compiler_copy_function_constants(cs, function) ||
            !compiler_copy_function_locals(cs, function) ||
            !compiler_copy_function_closures(cs, function)) {
            return ZR_FALSE;
        }
    }

    if (!compiler_copy_function_children(cs, function) ||
        !compiler_validate_function_child_graph(cs, function, sourceNode) ||
        !compiler_copy_function_exports(cs, function)) {
        return ZR_FALSE;
    }

    function->stackSize = (TZrUInt32) cs->maxStackSlotCount;
    if (!preserveExistingSignature) {
        function->parameterCount = 0;
        function->hasVariableArguments = ZR_FALSE;
    }
    function->lineInSourceStart = (sourceNode->location.start.line > 0) ? (TZrUInt32) sourceNode->location.start.line : 0;
    function->lineInSourceEnd = (sourceNode->location.end.line > 0) ? (TZrUInt32) sourceNode->location.end.line : 0;

    if (copyCurrentFunctionBuffers &&
        !compiler_copy_function_exception_metadata_slice(cs, function, 0, 0, 0, sourceNode)) {
        return ZR_FALSE;
    }

    compiler_finalize_optional_function_lengths(function);
    return ZR_TRUE;
}
