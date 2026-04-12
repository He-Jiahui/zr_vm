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
        if (sourceChildren[index] != ZR_NULL) {
            ZrCore_Function_DetachOwnedBuffers(sourceChildren[index]);
        }
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

static void compiler_free_function_escape_metadata(SZrGlobalState *global, SZrFunction *function) {
    if (global == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->escapeBindings != ZR_NULL && function->escapeBindingLength > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->escapeBindings,
                                      sizeof(SZrFunctionEscapeBinding) * function->escapeBindingLength,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }
    if (function->returnEscapeSlots != ZR_NULL && function->returnEscapeSlotCount > 0) {
        ZrCore_Memory_RawFreeWithType(global,
                                      function->returnEscapeSlots,
                                      sizeof(TZrUInt32) * function->returnEscapeSlotCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    }

    function->escapeBindings = ZR_NULL;
    function->escapeBindingLength = 0;
    function->returnEscapeSlots = ZR_NULL;
    function->returnEscapeSlotCount = 0;
}

static void compiler_reset_function_escape_sources(SZrFunction *function) {
    if (function == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        function->localVariableList[index].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE;
    }
    for (TZrUInt32 index = 0; index < function->closureValueLength; index++) {
        function->closureValueList[index].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
    }
}

static SZrFunctionLocalVariable *compiler_find_local_variable_by_slot(SZrFunction *function, TZrUInt32 slot) {
    if (function == ZR_NULL || function->localVariableList == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        if (function->localVariableList[index].stackSlot == slot) {
            return &function->localVariableList[index];
        }
    }

    return ZR_NULL;
}

static const SZrFunctionTypedLocalBinding *compiler_find_typed_local_binding_by_slot(const SZrFunction *function,
                                                                                     TZrUInt32 slot) {
    if (function == ZR_NULL || function->typedLocalBindings == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        if (binding->stackSlot == slot) {
            return binding;
        }
    }

    return ZR_NULL;
}

static TZrBool compiler_typed_binding_matches_name(const SZrFunctionTypedLocalBinding *binding, const char *typeName) {
    const TZrChar *bindingTypeName;

    if (binding == ZR_NULL || typeName == ZR_NULL || binding->type.typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingTypeName = ZrCore_String_GetNativeString(binding->type.typeName);
    return bindingTypeName != ZR_NULL && strcmp(bindingTypeName, typeName) == 0;
}

static const TZrChar *compiler_member_entry_symbol_text(const SZrFunction *function, TZrUInt16 memberEntryIndex) {
    if (function == ZR_NULL || function->memberEntries == ZR_NULL || memberEntryIndex >= function->memberEntryLength) {
        return ZR_NULL;
    }

    if (function->memberEntries[memberEntryIndex].symbol == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(function->memberEntries[memberEntryIndex].symbol);
}

static TZrBool compiler_function_constant_is_native_callable(const SZrFunction *function, TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (function == ZR_NULL || function->constantValueList == ZR_NULL || constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    if (constantValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (constantValue->type) {
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return constantValue->isNative;
        case ZR_VALUE_TYPE_NATIVE_POINTER:
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}

static const TZrInstruction *compiler_find_latest_slot_writer_before_instruction(const SZrFunction *function,
                                                                                 TZrUInt32 instructionIndex,
                                                                                 TZrUInt32 slot);

static TZrBool compiler_result_slot_flows_to_callback_handle_binding(const SZrFunction *function,
                                                                     TZrUInt32 instructionIndex,
                                                                     TZrUInt32 resultSlot) {
    TZrUInt32 slot = resultSlot;

    if (compiler_typed_binding_matches_name(compiler_find_typed_local_binding_by_slot(function, slot), "CallbackHandle")) {
        return ZR_TRUE;
    }

    for (TZrUInt32 scanIndex = instructionIndex + 1u;
         function != ZR_NULL && function->instructionsList != ZR_NULL && scanIndex < function->instructionsLength &&
         scanIndex <= instructionIndex + 4u;
         scanIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[scanIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;

        if ((opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) &&
            (TZrUInt32)instruction->instruction.operand.operand2[0] == slot) {
            slot = instruction->instruction.operandExtra;
            if (compiler_typed_binding_matches_name(compiler_find_typed_local_binding_by_slot(function, slot),
                                                    "CallbackHandle")) {
                return ZR_TRUE;
            }
            continue;
        }

        if (instruction->instruction.operandExtra == slot) {
            break;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_slot_resolves_to_native_callback_factory_before_instruction(const SZrFunction *function,
                                                                                     TZrUInt32 instructionIndex,
                                                                                     TZrUInt32 slot,
                                                                                     TZrUInt32 recursionDepth) {
    const TZrInstruction *writer;
    EZrInstructionCode opcode;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        instructionIndex > function->instructionsLength || recursionDepth > function->instructionsLength) {
        return ZR_FALSE;
    }

    writer = compiler_find_latest_slot_writer_before_instruction(function, instructionIndex, slot);
    if (writer == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)writer->instruction.operationCode;
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            return compiler_function_constant_is_native_callable(
                    function,
                    (TZrUInt32)writer->instruction.operand.operand2[0]);
        case ZR_INSTRUCTION_ENUM(GET_STACK):
        case ZR_INSTRUCTION_ENUM(SET_STACK): {
            TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
            if (sourceSlot == slot) {
                return ZR_FALSE;
            }

            return compiler_slot_resolves_to_native_callback_factory_before_instruction(function,
                                                                                         (TZrUInt32)(writer - function->instructionsList),
                                                                                         sourceSlot,
                                                                                         recursionDepth + 1u);
        }
        case ZR_INSTRUCTION_ENUM(GET_MEMBER): {
            const TZrChar *memberName =
                    compiler_member_entry_symbol_text(function, writer->instruction.operand.operand1[1]);
            return memberName != ZR_NULL && strcmp(memberName, "callback") == 0;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool compiler_instruction_is_native_callback_handle_creation(const SZrFunction *function,
                                                                       TZrUInt32 instructionIndex,
                                                                       const TZrInstruction *instruction) {
    EZrInstructionCode opcode;

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode != ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL) &&
        opcode != ZR_INSTRUCTION_ENUM(FUNCTION_CALL)) {
        return ZR_FALSE;
    }

    if (!compiler_result_slot_flows_to_callback_handle_binding(function,
                                                               instructionIndex,
                                                               instruction->instruction.operandExtra)) {
        return ZR_FALSE;
    }

    if (opcode == ZR_INSTRUCTION_ENUM(KNOWN_NATIVE_CALL)) {
        return ZR_TRUE;
    }

    return compiler_slot_resolves_to_native_callback_factory_before_instruction(
            function,
            instructionIndex,
            instruction->instruction.operand.operand1[0],
            0u);
}

static const TZrInstruction *compiler_find_latest_slot_writer_before_instruction(const SZrFunction *function,
                                                                                 TZrUInt32 instructionIndex,
                                                                                 TZrUInt32 slot) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL || instructionIndex == 0) {
        return ZR_NULL;
    }

    for (TZrUInt32 reverseIndex = instructionIndex; reverseIndex > 0; reverseIndex--) {
        const TZrInstruction *instruction = &function->instructionsList[reverseIndex - 1];
        if (instruction->instruction.operandExtra == slot) {
            return instruction;
        }
    }

    return ZR_NULL;
}

static TZrUInt32 compiler_find_callable_child_index_from_constant(const SZrFunction *function,
                                                                  TZrUInt32 constantIndex) {
    if (function == ZR_NULL || function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (compiler_constant_refs_function(&function->constantValueList[constantIndex],
                                            &function->childFunctionList[childIndex])) {
            return childIndex;
        }
    }

    return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
}

static TZrUInt32 compiler_find_callable_child_index_by_name(const SZrFunction *function, const SZrString *name) {
    const TZrChar *targetName;

    if (function == ZR_NULL || name == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    }

    targetName = ZrCore_String_GetNativeString(name);
    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        SZrString *childNameObject = function->childFunctionList[childIndex].functionName;
        const TZrChar *childName = childNameObject != ZR_NULL ? ZrCore_String_GetNativeString(childNameObject) : ZR_NULL;

        if (childNameObject == name) {
            return childIndex;
        }

        if (targetName != ZR_NULL && childName != ZR_NULL && strcmp(targetName, childName) == 0) {
            return childIndex;
        }
    }

    return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
}

static TZrUInt32 compiler_resolve_callable_child_index_before_instruction(const SZrFunction *function,
                                                                          const SZrFunction *parentFunction,
                                                                          TZrUInt32 instructionLimit,
                                                                          TZrUInt32 slot,
                                                                          TZrUInt32 recursionDepth) {
    TZrUInt32 scanIndex;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL ||
        recursionDepth > function->instructionsLength) {
        return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    }

    for (scanIndex = instructionLimit; scanIndex > 0; scanIndex--) {
        const TZrInstruction *instruction = &function->instructionsList[scanIndex - 1];

        if (instruction->instruction.operandExtra != slot) {
            continue;
        }

        switch ((EZrInstructionCode)instruction->instruction.operationCode) {
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
                return (TZrUInt32)instruction->instruction.operand.operand1[0];
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                return compiler_find_callable_child_index_from_constant(
                        function,
                        (TZrUInt32)instruction->instruction.operand.operand2[0]);
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                return compiler_find_callable_child_index_from_constant(
                        function,
                        (TZrUInt32)instruction->instruction.operand.operand1[0]);
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK): {
                TZrUInt32 sourceSlot = (TZrUInt32)instruction->instruction.operand.operand2[0];
                if (sourceSlot == slot) {
                    return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
                }
                return compiler_resolve_callable_child_index_before_instruction(function,
                                                                                parentFunction,
                                                                                scanIndex - 1,
                                                                                sourceSlot,
                                                                                recursionDepth + 1);
            }
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                if (parentFunction != ZR_NULL &&
                    function->closureValueList != ZR_NULL &&
                    instruction->instruction.operand.operand1[0] < function->closureValueLength) {
                    const SZrFunctionClosureVariable *closure =
                            &function->closureValueList[instruction->instruction.operand.operand1[0]];
                    if (closure->inStack) {
                        return compiler_resolve_callable_child_index_before_instruction(parentFunction,
                                                                                        ZR_NULL,
                                                                                        parentFunction->instructionsLength,
                                                                                        closure->index,
                                                                                        recursionDepth + 1);
                    }
                }
                return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
            default:
                return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        }
    }

    return ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
}

static TZrBool compiler_slot_is_global_root_receiver(const SZrFunction *function,
                                                     TZrUInt32 instructionIndex,
                                                     TZrUInt32 slot) {
    TZrUInt32 depth = 0;
    const TZrInstruction *writer =
            compiler_find_latest_slot_writer_before_instruction(function, instructionIndex, slot);

    while (writer != ZR_NULL && depth < 8) {
        EZrInstructionCode opcode = (EZrInstructionCode)writer->instruction.operationCode;

        if (opcode == ZR_INSTRUCTION_ENUM(GET_GLOBAL)) {
            return ZR_TRUE;
        }

        if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
            slot = (TZrUInt32)writer->instruction.operand.operand2[0];
            writer = compiler_find_latest_slot_writer_before_instruction(
                    function,
                    (TZrUInt32)(writer - function->instructionsList),
                    slot);
            depth++;
            continue;
        }

        break;
    }

    return ZR_FALSE;
}

static TZrUInt32 compiler_resolve_copy_source_slot_before_instruction(const SZrFunction *function,
                                                                      TZrUInt32 instructionIndex,
                                                                      TZrUInt32 slot) {
    TZrUInt32 depth = 0;
    const TZrInstruction *writer =
            compiler_find_latest_slot_writer_before_instruction(function, instructionIndex, slot);

    while (writer != ZR_NULL && depth < 8) {
        EZrInstructionCode opcode = (EZrInstructionCode)writer->instruction.operationCode;

        if (compiler_find_local_variable_by_slot((SZrFunction *)function, slot) != ZR_NULL) {
            break;
        }

        if (opcode != ZR_INSTRUCTION_ENUM(GET_STACK) &&
            opcode != ZR_INSTRUCTION_ENUM(SET_STACK)) {
            break;
        }

        slot = (TZrUInt32)writer->instruction.operand.operand2[0];
        writer = compiler_find_latest_slot_writer_before_instruction(
                function,
                (TZrUInt32)(writer - function->instructionsList),
                slot);
        depth++;
    }

    return slot;
}

static TZrBool compiler_return_slot_exists(const TZrUInt32 *slots, TZrUInt32 count, TZrUInt32 slot) {
    for (TZrUInt32 index = 0; index < count; index++) {
        if (slots[index] == slot) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool compiler_collect_return_escape_slots(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global;
    TZrUInt32 *collectedSlots;
    TZrUInt32 slotCount = 0;

    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->instructionsLength == 0) {
        return ZR_TRUE;
    }

    global = cs->state->global;
    collectedSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                  sizeof(TZrUInt32) * function->instructionsLength,
                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (collectedSlots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 returnCount;
        TZrUInt32 resultSlot;

        if (opcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            continue;
        }

        returnCount = instruction->instruction.operandExtra;
        resultSlot = instruction->instruction.operand.operand1[0];
        if (returnCount == 0 || compiler_return_slot_exists(collectedSlots, slotCount, resultSlot)) {
            continue;
        }

        collectedSlots[slotCount++] = resultSlot;
    }

    if (slotCount > 0) {
        function->returnEscapeSlots = (TZrUInt32 *)ZrCore_Memory_RawMallocWithType(global,
                                                                                    sizeof(TZrUInt32) * slotCount,
                                                                                    ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->returnEscapeSlots == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global,
                                          collectedSlots,
                                          sizeof(TZrUInt32) * function->instructionsLength,
                                          ZR_MEMORY_NATIVE_TYPE_FUNCTION);
            return ZR_FALSE;
        }

        memcpy(function->returnEscapeSlots, collectedSlots, sizeof(TZrUInt32) * slotCount);
        function->returnEscapeSlotCount = slotCount;
    }

    ZrCore_Memory_RawFreeWithType(global,
                                  collectedSlots,
                                  sizeof(TZrUInt32) * function->instructionsLength,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return ZR_TRUE;
}

static void compiler_mark_return_escape_locals(SZrFunction *function) {
    if (function == ZR_NULL || function->returnEscapeSlots == ZR_NULL) {
        return;
    }

    for (TZrUInt32 slotIndex = 0; slotIndex < function->returnEscapeSlotCount; slotIndex++) {
        SZrFunctionLocalVariable *local =
                compiler_find_local_variable_by_slot(function, function->returnEscapeSlots[slotIndex]);
        if (local != ZR_NULL) {
            local->escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN;
        }
    }
}

static void compiler_mark_module_export_locals(SZrFunction *function) {
    if (function == ZR_NULL || function->exportedVariables == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        SZrFunctionLocalVariable *local =
                compiler_find_local_variable_by_slot(function, function->exportedVariables[index].stackSlot);
        if (local != ZR_NULL) {
            local->escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT;
        }
    }
}

static void compiler_mark_child_capture_escape_flags_from_parent(SZrFunction *parent,
                                                                 SZrFunction *child,
                                                                 TZrUInt32 escapeFlags) {
    if (parent == ZR_NULL || child == ZR_NULL || child->closureValueList == ZR_NULL ||
        escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    for (TZrUInt32 closureIndex = 0; closureIndex < child->closureValueLength; closureIndex++) {
        SZrFunctionClosureVariable *closure = &child->closureValueList[closureIndex];

        closure->escapeFlags |= escapeFlags;
        if (closure->inStack) {
            SZrFunctionLocalVariable *local = compiler_find_local_variable_by_slot(parent, closure->index);
            if (local != ZR_NULL) {
                local->escapeFlags |= escapeFlags;
            }
        } else if (parent->closureValueList != ZR_NULL && closure->index < parent->closureValueLength) {
            parent->closureValueList[closure->index].escapeFlags |= escapeFlags;
        }
    }
}

static void compiler_mark_module_export_callable_child_captures(SZrFunction *function) {
    if (function == ZR_NULL || function->exportedVariables == ZR_NULL || function->childFunctionList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];

        if (exported->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            exported->callableChildIndex >= function->childFunctionLength) {
            continue;
        }

        compiler_mark_child_capture_escape_flags_from_parent(function,
                                                             &function->childFunctionList[exported->callableChildIndex],
                                                             ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT);
    }
}

static TZrBool compiler_mark_parent_callable_child_captures_from_upvalue(SZrFunction *function,
                                                                         const SZrFunction *parentFunction,
                                                                         TZrUInt32 instructionIndex,
                                                                         TZrUInt32 slot,
                                                                         TZrUInt32 escapeFlags,
                                                                         TZrUInt32 recursionDepth);

static void compiler_mark_callable_child_captures_for_slot(SZrFunction *function,
                                                           const SZrFunction *parentFunction,
                                                           TZrUInt32 instructionIndex,
                                                           TZrUInt32 sourceSlot,
                                                           TZrUInt32 escapeFlags) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return;
    }

    childIndex = compiler_resolve_callable_child_index_before_instruction(function,
                                                                          parentFunction,
                                                                          instructionIndex,
                                                                          sourceSlot,
                                                                          0u);
    if (childIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
        childIndex >= function->childFunctionLength) {
        compiler_mark_parent_callable_child_captures_from_upvalue(function,
                                                                  parentFunction,
                                                                  instructionIndex,
                                                                  sourceSlot,
                                                                  escapeFlags,
                                                                  0u);
        return;
    }

    compiler_mark_child_capture_escape_flags_from_parent(function,
                                                         &function->childFunctionList[childIndex],
                                                         escapeFlags);
}

static TZrBool compiler_instruction_is_global_binding_write(const SZrFunction *function,
                                                            TZrUInt32 instructionIndex,
                                                            const TZrInstruction *instruction,
                                                            TZrUInt32 *outSourceSlot);
static TZrBool compiler_mark_parent_callable_child_captures_from_upvalue(SZrFunction *function,
                                                                         const SZrFunction *parentFunction,
                                                                         TZrUInt32 instructionIndex,
                                                                         TZrUInt32 slot,
                                                                         TZrUInt32 escapeFlags,
                                                                         TZrUInt32 recursionDepth);

static TZrBool compiler_mark_parent_callable_child_captures_from_upvalue(SZrFunction *function,
                                                                         const SZrFunction *parentFunction,
                                                                         TZrUInt32 instructionIndex,
                                                                         TZrUInt32 slot,
                                                                         TZrUInt32 escapeFlags,
                                                                         TZrUInt32 recursionDepth) {
    const TZrInstruction *writer;
    EZrInstructionCode opcode;

    if (function == ZR_NULL || parentFunction == ZR_NULL || function->instructionsList == ZR_NULL ||
        recursionDepth > function->instructionsLength || escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
        return ZR_FALSE;
    }

    writer = compiler_find_latest_slot_writer_before_instruction(function, instructionIndex, slot);
    if (writer == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)writer->instruction.operationCode;
    if (opcode == ZR_INSTRUCTION_ENUM(GET_STACK) || opcode == ZR_INSTRUCTION_ENUM(SET_STACK)) {
        TZrUInt32 sourceSlot = (TZrUInt32)writer->instruction.operand.operand2[0];
        if (sourceSlot == slot) {
            return ZR_FALSE;
        }
        return compiler_mark_parent_callable_child_captures_from_upvalue(function,
                                                                         parentFunction,
                                                                         (TZrUInt32)(writer - function->instructionsList),
                                                                         sourceSlot,
                                                                         escapeFlags,
                                                                         recursionDepth + 1u);
    }

    if (opcode == ZR_INSTRUCTION_ENUM(GETUPVAL) &&
        function->closureValueList != ZR_NULL &&
        writer->instruction.operand.operand1[0] < function->closureValueLength) {
        TZrUInt32 closureIndex = writer->instruction.operand.operand1[0];
        SZrFunctionClosureVariable *closure = &function->closureValueList[closureIndex];
        TZrUInt32 childIndex = compiler_find_callable_child_index_by_name(parentFunction, closure->name);

        closure->escapeFlags |= escapeFlags;
        if (childIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE && closure->inStack) {
            childIndex = compiler_resolve_callable_child_index_before_instruction(parentFunction,
                                                                                  ZR_NULL,
                                                                                  parentFunction->instructionsLength,
                                                                                  closure->index,
                                                                                  recursionDepth + 1u);
        }

        if (childIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            childIndex >= parentFunction->childFunctionLength) {
            return ZR_FALSE;
        }

        compiler_mark_child_capture_escape_flags_from_parent(ZR_CAST(SZrFunction *, parentFunction),
                                                             &ZR_CAST(SZrFunction *, parentFunction)
                                                                     ->childFunctionList[childIndex],
                                                             escapeFlags);
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void compiler_mark_global_binding_locals(SZrFunction *function) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        TZrUInt32 sourceSlot = 0;
        SZrFunctionLocalVariable *local;

        if (!compiler_instruction_is_global_binding_write(function,
                                                          instructionIndex,
                                                          instruction,
                                                          &sourceSlot)) {
            continue;
        }

        local = compiler_find_local_variable_by_slot(function, sourceSlot);
        if (local != ZR_NULL) {
            local->escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
        }
    }
}

static void compiler_mark_return_callable_child_captures(SZrFunction *function, const SZrFunction *parentFunction) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        EZrInstructionCode opcode = (EZrInstructionCode)instruction->instruction.operationCode;
        TZrUInt32 returnCount;
        TZrUInt32 resultSlot;

        if (opcode != ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)) {
            continue;
        }

        returnCount = instruction->instruction.operandExtra;
        resultSlot = instruction->instruction.operand.operand1[0];
        if (returnCount == 0u) {
            continue;
        }

        compiler_mark_callable_child_captures_for_slot(function,
                                                       parentFunction,
                                                       instructionIndex,
                                                       resultSlot,
                                                       ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN);
    }
}

static void compiler_mark_global_binding_callable_child_captures(SZrFunction *function,
                                                                 const SZrFunction *parentFunction) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        TZrUInt32 sourceSlot = 0u;

        if (!compiler_instruction_is_global_binding_write(function,
                                                          instructionIndex,
                                                          &function->instructionsList[instructionIndex],
                                                          &sourceSlot)) {
            continue;
        }

        compiler_mark_callable_child_captures_for_slot(function,
                                                       parentFunction,
                                                       instructionIndex,
                                                       sourceSlot,
                                                       ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT);
    }
}

static void compiler_mark_native_binding_callable_child_captures(SZrFunction *function,
                                                                 const SZrFunction *parentFunction) {
    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        const TZrInstruction *instruction = &function->instructionsList[instructionIndex];
        TZrUInt32 functionSlot;
        TZrUInt32 argumentCount;

        if (!compiler_instruction_is_native_callback_handle_creation(function, instructionIndex, instruction)) {
            continue;
        }

        functionSlot = (TZrUInt32)instruction->instruction.operand.operand1[0];
        argumentCount = (TZrUInt32)instruction->instruction.operand.operand1[1];
        for (TZrUInt32 argumentIndex = 0; argumentIndex < argumentCount; argumentIndex++) {
            TZrUInt32 sourceSlot =
                    compiler_resolve_copy_source_slot_before_instruction(function,
                                                                        instructionIndex,
                                                                        functionSlot + 1u + argumentIndex);
            SZrFunctionLocalVariable *local;

            if (compiler_resolve_callable_child_index_before_instruction(function,
                                                                         parentFunction,
                                                                         instructionIndex,
                                                                         sourceSlot,
                                                                         0u) ==
                ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE) {
                continue;
            }

            local = compiler_find_local_variable_by_slot(function, sourceSlot);
            if (local != ZR_NULL) {
                local->escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE;
            }

            compiler_mark_callable_child_captures_for_slot(function,
                                                           parentFunction,
                                                           instructionIndex,
                                                           sourceSlot,
                                                           ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE);
        }
    }
}

static void compiler_mark_parent_capture_from_child(SZrFunction *function, const SZrFunction *child) {
    if (function == ZR_NULL || child == ZR_NULL || child->closureValueList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 closureIndex = 0; closureIndex < child->closureValueLength; closureIndex++) {
        const SZrFunctionClosureVariable *closure = &child->closureValueList[closureIndex];

        if (closure->inStack) {
            SZrFunctionLocalVariable *local = compiler_find_local_variable_by_slot(function, closure->index);
            if (local != ZR_NULL) {
                local->escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
            }
        } else if (function->closureValueList != ZR_NULL && closure->index < function->closureValueLength) {
            function->closureValueList[closure->index].escapeFlags |= ZR_GARBAGE_COLLECT_ESCAPE_KIND_CLOSURE_CAPTURE;
        }
    }
}

static TZrUInt32 compiler_resolve_parent_escape_scope_depth(TZrUInt32 currentScopeDepth, TZrUInt32 parentScopeDepth) {
    if (parentScopeDepth != ZR_GC_SCOPE_DEPTH_NONE) {
        return parentScopeDepth;
    }

    return currentScopeDepth;
}

static void compiler_propagate_parent_escape_flags_to_child(const SZrFunction *parent, SZrFunction *child) {
    if (parent == ZR_NULL || child == ZR_NULL || child->closureValueList == ZR_NULL) {
        return;
    }

    for (TZrUInt32 closureIndex = 0; closureIndex < child->closureValueLength; closureIndex++) {
        SZrFunctionClosureVariable *closure = &child->closureValueList[closureIndex];

        if (closure->inStack) {
            const SZrFunctionLocalVariable *local =
                    compiler_find_local_variable_by_slot((SZrFunction *)parent, closure->index);
            if (local != ZR_NULL) {
                closure->escapeFlags |= local->escapeFlags;
                closure->scopeDepth =
                        compiler_resolve_parent_escape_scope_depth(closure->scopeDepth, local->scopeDepth);
            }
        } else if (parent->closureValueList != ZR_NULL && closure->index < parent->closureValueLength) {
            const SZrFunctionClosureVariable *parentClosure = &parent->closureValueList[closure->index];
            closure->escapeFlags |= parentClosure->escapeFlags;
            closure->scopeDepth =
                    compiler_resolve_parent_escape_scope_depth(closure->scopeDepth, parentClosure->scopeDepth);
        }
    }
}

static TZrBool compiler_escape_binding_exists(const SZrFunctionEscapeBinding *bindings,
                                              TZrUInt32 bindingCount,
                                              EZrFunctionEscapeBindingKind kind,
                                              TZrUInt32 slotOrIndex) {
    if (bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < bindingCount; index++) {
        const SZrFunctionEscapeBinding *binding = &bindings[index];
        if ((EZrFunctionEscapeBindingKind)binding->bindingKind == kind &&
            binding->slotOrIndex == slotOrIndex) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrUInt32 compiler_count_synthetic_return_escape_bindings(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->returnEscapeSlots == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->returnEscapeSlotCount; index++) {
        TZrUInt32 slot = function->returnEscapeSlots[index];
        TZrBool seenEarlier = ZR_FALSE;

        if (compiler_find_local_variable_by_slot((SZrFunction *)function, slot) != ZR_NULL) {
            continue;
        }

        for (TZrUInt32 previous = 0; previous < index; previous++) {
            if (function->returnEscapeSlots[previous] == slot) {
                seenEarlier = ZR_TRUE;
                break;
            }
        }

        if (!seenEarlier) {
            count++;
        }
    }

    return count;
}

static TZrBool compiler_instruction_is_global_binding_write(const SZrFunction *function,
                                                            TZrUInt32 instructionIndex,
                                                            const TZrInstruction *instruction,
                                                            TZrUInt32 *outSourceSlot) {
    EZrInstructionCode opcode;
    TZrUInt32 receiverSlot;

    if (outSourceSlot != ZR_NULL) {
        *outSourceSlot = 0;
    }

    if (function == ZR_NULL || instruction == ZR_NULL) {
        return ZR_FALSE;
    }

    opcode = (EZrInstructionCode)instruction->instruction.operationCode;
    if (opcode != ZR_INSTRUCTION_ENUM(SET_MEMBER) &&
        opcode != ZR_INSTRUCTION_ENUM(SET_MEMBER_SLOT)) {
        return ZR_FALSE;
    }

    receiverSlot = (TZrUInt32)instruction->instruction.operand.operand1[0];
    if (!compiler_slot_is_global_root_receiver(function, instructionIndex, receiverSlot)) {
        return ZR_FALSE;
    }

    if (outSourceSlot != ZR_NULL) {
        *outSourceSlot = compiler_resolve_copy_source_slot_before_instruction(
                function,
                instructionIndex,
                instruction->instruction.operandExtra);
    }
    return ZR_TRUE;
}

static TZrUInt32 compiler_count_synthetic_global_escape_bindings(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->instructionsList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        TZrUInt32 sourceSlot = 0;
        TZrBool seenEarlier = ZR_FALSE;

        if (!compiler_instruction_is_global_binding_write(function,
                                                          instructionIndex,
                                                          &function->instructionsList[instructionIndex],
                                                          &sourceSlot)) {
            continue;
        }

        if (compiler_find_local_variable_by_slot((SZrFunction *)function, sourceSlot) != ZR_NULL) {
            continue;
        }

        for (TZrUInt32 previous = 0; previous < instructionIndex; previous++) {
            TZrUInt32 previousSourceSlot = 0;
            if (compiler_instruction_is_global_binding_write(function,
                                                             previous,
                                                             &function->instructionsList[previous],
                                                             &previousSourceSlot) &&
                previousSourceSlot == sourceSlot) {
                seenEarlier = ZR_TRUE;
                break;
            }
        }

        if (!seenEarlier) {
            count++;
        }
    }

    return count;
}

static TZrUInt32 compiler_count_synthetic_native_escape_bindings(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL || function->localVariableList == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];

        if ((local->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) == 0u) {
            continue;
        }

        if (compiler_resolve_callable_child_index_before_instruction(function,
                                                                     ZR_NULL,
                                                                     function->instructionsLength,
                                                                     local->stackSlot,
                                                                     0u) ==
            ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE) {
            continue;
        }

        count++;
    }

    return count;
}

static TZrUInt32 compiler_count_escape_bindings(const SZrFunction *function) {
    TZrUInt32 count = 0;

    if (function == ZR_NULL) {
        return 0;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        if (function->localVariableList[index].escapeFlags != ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
            count++;
        }
    }
    for (TZrUInt32 index = 0; index < function->closureValueLength; index++) {
        if (function->closureValueList[index].escapeFlags != ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
            count++;
        }
    }

    return count + function->exportedVariableLength +
           compiler_count_synthetic_return_escape_bindings(function) +
           compiler_count_synthetic_global_escape_bindings(function) +
           compiler_count_synthetic_native_escape_bindings(function);
}

static TZrBool compiler_build_escape_bindings(SZrCompilerState *cs, SZrFunction *function) {
    SZrGlobalState *global;
    TZrUInt32 bindingCount;
    TZrUInt32 nextBinding = 0;

    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingCount = compiler_count_escape_bindings(function);
    if (bindingCount == 0) {
        return ZR_TRUE;
    }

    global = cs->state->global;
    function->escapeBindings = (SZrFunctionEscapeBinding *)ZrCore_Memory_RawMallocWithType(
            global,
            sizeof(SZrFunctionEscapeBinding) * bindingCount,
            ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (function->escapeBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Memory_RawSet(function->escapeBindings, 0, sizeof(SZrFunctionEscapeBinding) * bindingCount);
    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];
        if (local->escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
            continue;
        }

        function->escapeBindings[nextBinding].name = local->name;
        function->escapeBindings[nextBinding].slotOrIndex = local->stackSlot;
        function->escapeBindings[nextBinding].scopeDepth = local->scopeDepth;
        function->escapeBindings[nextBinding].escapeFlags = local->escapeFlags;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_LOCAL;
        nextBinding++;
    }

    for (TZrUInt32 index = 0; index < function->closureValueLength; index++) {
        const SZrFunctionClosureVariable *closure = &function->closureValueList[index];
        if (closure->escapeFlags == ZR_GARBAGE_COLLECT_ESCAPE_KIND_NONE) {
            continue;
        }

        function->escapeBindings[nextBinding].name = closure->name;
        function->escapeBindings[nextBinding].slotOrIndex = index;
        function->escapeBindings[nextBinding].scopeDepth = closure->scopeDepth;
        function->escapeBindings[nextBinding].escapeFlags = closure->escapeFlags;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_CLOSURE;
        nextBinding++;
    }

    for (TZrUInt32 index = 0; index < function->exportedVariableLength; index++) {
        const SZrFunctionExportedVariable *exported = &function->exportedVariables[index];
        SZrFunctionLocalVariable *local = compiler_find_local_variable_by_slot(function, exported->stackSlot);

        function->escapeBindings[nextBinding].name = exported->name;
        function->escapeBindings[nextBinding].slotOrIndex = exported->stackSlot;
        function->escapeBindings[nextBinding].scopeDepth = local != ZR_NULL ? local->scopeDepth : 0u;
        function->escapeBindings[nextBinding].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_MODULE_ROOT;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_MODULE_EXPORT;
        nextBinding++;
    }

    for (TZrUInt32 index = 0; index < function->returnEscapeSlotCount; index++) {
        TZrUInt32 slot = function->returnEscapeSlots[index];

        if (compiler_find_local_variable_by_slot(function, slot) != ZR_NULL ||
            compiler_escape_binding_exists(function->escapeBindings,
                                           nextBinding,
                                           ZR_FUNCTION_ESCAPE_BINDING_KIND_RETURN_SLOT,
                                           slot)) {
            continue;
        }

        function->escapeBindings[nextBinding].name = ZR_NULL;
        function->escapeBindings[nextBinding].slotOrIndex = slot;
        function->escapeBindings[nextBinding].scopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
        function->escapeBindings[nextBinding].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_RETURN;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_RETURN_SLOT;
        nextBinding++;
    }

    for (TZrUInt32 instructionIndex = 0; instructionIndex < function->instructionsLength; instructionIndex++) {
        TZrUInt32 sourceSlot = 0;

        if (!compiler_instruction_is_global_binding_write(function,
                                                          instructionIndex,
                                                          &function->instructionsList[instructionIndex],
                                                          &sourceSlot) ||
            compiler_find_local_variable_by_slot(function, sourceSlot) != ZR_NULL ||
            compiler_escape_binding_exists(function->escapeBindings,
                                           nextBinding,
                                           ZR_FUNCTION_ESCAPE_BINDING_KIND_GLOBAL_BINDING,
                                           sourceSlot)) {
            continue;
        }

        function->escapeBindings[nextBinding].name = ZR_NULL;
        function->escapeBindings[nextBinding].slotOrIndex = sourceSlot;
        function->escapeBindings[nextBinding].scopeDepth = ZR_GC_SCOPE_DEPTH_NONE;
        function->escapeBindings[nextBinding].escapeFlags = ZR_GARBAGE_COLLECT_ESCAPE_KIND_GLOBAL_ROOT;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_GLOBAL_BINDING;
        nextBinding++;
    }

    for (TZrUInt32 index = 0; index < function->localVariableLength; index++) {
        const SZrFunctionLocalVariable *local = &function->localVariableList[index];

        if ((local->escapeFlags & ZR_GARBAGE_COLLECT_ESCAPE_KIND_NATIVE_HANDLE) == 0u ||
            compiler_resolve_callable_child_index_before_instruction(function,
                                                                     ZR_NULL,
                                                                     function->instructionsLength,
                                                                     local->stackSlot,
                                                                     0u) ==
                    ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            compiler_escape_binding_exists(function->escapeBindings,
                                           nextBinding,
                                           ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING,
                                           local->stackSlot)) {
            continue;
        }

        function->escapeBindings[nextBinding].name = local->name;
        function->escapeBindings[nextBinding].slotOrIndex = local->stackSlot;
        function->escapeBindings[nextBinding].scopeDepth = local->scopeDepth;
        function->escapeBindings[nextBinding].escapeFlags = local->escapeFlags;
        function->escapeBindings[nextBinding].bindingKind = ZR_FUNCTION_ESCAPE_BINDING_KIND_NATIVE_BINDING;
        nextBinding++;
    }

    function->escapeBindingLength = nextBinding;
    return ZR_TRUE;
}

static TZrBool compiler_prepare_function_escape_metadata_recursive(SZrCompilerState *cs, SZrFunction *function) {
    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    compiler_free_function_escape_metadata(cs->state->global, function);
    compiler_reset_function_escape_sources(function);

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_prepare_function_escape_metadata_recursive(cs, &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_collect_function_escape_sources_recursive(SZrCompilerState *cs,
                                                                  SZrFunction *function,
                                                                  const SZrFunction *parentFunction) {
    if (cs == ZR_NULL || function == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_collect_function_escape_sources_recursive(cs,
                                                                &function->childFunctionList[childIndex],
                                                                function)) {
            return ZR_FALSE;
        }
    }

    if (!compiler_collect_return_escape_slots(cs, function)) {
        return ZR_FALSE;
    }

    compiler_mark_return_escape_locals(function);
    compiler_mark_module_export_locals(function);
    compiler_mark_module_export_callable_child_captures(function);
    compiler_mark_global_binding_locals(function);
    compiler_mark_return_callable_child_captures(function, parentFunction);
    compiler_mark_global_binding_callable_child_captures(function, parentFunction);
    compiler_mark_native_binding_callable_child_captures(function, parentFunction);
    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        compiler_mark_parent_capture_from_child(function, &function->childFunctionList[childIndex]);
    }

    return ZR_TRUE;
}

static TZrBool compiler_propagate_function_escape_metadata_recursive(SZrFunction *function) {
    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        SZrFunction *child = &function->childFunctionList[childIndex];

        compiler_propagate_parent_escape_flags_to_child(function, child);
        if (!compiler_propagate_function_escape_metadata_recursive(child)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool compiler_build_escape_bindings_recursive(SZrCompilerState *cs, SZrFunction *function) {
    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (!compiler_build_escape_bindings_recursive(cs, &function->childFunctionList[childIndex])) {
            return ZR_FALSE;
        }
    }

    return compiler_build_escape_bindings(cs, function);
}

static TZrBool compiler_finalize_function_escape_metadata_recursive(SZrCompilerState *cs, SZrFunction *function) {
    if (!compiler_prepare_function_escape_metadata_recursive(cs, function)) {
        return ZR_FALSE;
    }
    if (!compiler_collect_function_escape_sources_recursive(cs, function, ZR_NULL)) {
        return ZR_FALSE;
    }
    if (!compiler_propagate_function_escape_metadata_recursive(function)) {
        return ZR_FALSE;
    }
    return compiler_build_escape_bindings_recursive(cs, function);
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

ZR_PARSER_API TZrBool compiler_assemble_final_function(SZrCompilerState *cs,
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

    if (!compiler_finalize_function_escape_metadata_recursive(cs, function)) {
        return ZR_FALSE;
    }

    compiler_finalize_optional_function_lengths(function);
    return ZR_TRUE;
}
