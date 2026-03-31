//
// Runtime conversion helpers for .zro IO sources.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/ownership.h"

static void io_runtime_init_inline_function(SZrFunction *function) {
    ZrCore_Memory_RawSet(function, 0, sizeof(*function));
    ZrCore_RawObject_Construct(&function->super, ZR_RAW_OBJECT_TYPE_FUNCTION);
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

    nativeHelper = ZrCore_Io_GetSerializableNativeHelperFunction(source->startLine);

    switch (source->type) {
        case ZR_VALUE_TYPE_NULL:
            ZrCore_Value_ResetAsNull(destination);
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

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            if (nativeHelper == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsNativePointer(state, destination, ZR_CAST_PTR(nativeHelper));
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

    if (source->closuresLength > 0) {
        TZrSize childBytes = sizeof(SZrFunction) * source->closuresLength;
        function->childFunctionList =
                (SZrFunction *)ZrCore_Memory_RawMallocWithType(global, childBytes, ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->childFunctionList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < source->closuresLength; index++) {
            io_runtime_init_inline_function(&function->childFunctionList[index]);
            if (source->closures[index].subFunction == ZR_NULL ||
                !io_runtime_populate_function(state,
                                              source->closures[index].subFunction,
                                              &function->childFunctionList[index])) {
                return ZR_FALSE;
            }
        }
        function->childFunctionLength = (TZrUInt32)source->closuresLength;
    }

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
