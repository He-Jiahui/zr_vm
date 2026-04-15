//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"

#define ZR_IO_MALLOC_NATIVE_DATA(GLOBAL, SIZE) ZrCore_Memory_RawMallocWithType(GLOBAL, (SIZE), ZR_MEMORY_NATIVE_TYPE_IO);
#define ZR_IO_FREE_NATIVE_DATA(GLOBAL, DATA, SIZE)                                                                     \
    ZrCore_Memory_RawFreeWithType(GLOBAL, (DATA), (SIZE), ZR_MEMORY_NATIVE_TYPE_IO);
static TZrBool io_refill(SZrIo *io) {
    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TZrBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }
    io->remained = readSize;
    io->pointer = buffer;
    return ZR_TRUE;
}

static TZrInt32 io_read_char(SZrIo *io) {
    if (io->remained == 0) {
        if (!io_refill(io)) {
            // EOF = -1
            return ZR_IO_EOF;
        }
    }
    // BYTE > 0 && BYTE < 256
    TZrInt32 c = *io->pointer;
    io->remained--;
    io->pointer++;
    return c;
}

static ZR_FORCE_INLINE TZrSize io_read_size(SZrIo *io) {
    TZrSize size;
    ZrCore_Io_Read(io, (TZrBytePtr) &size, sizeof(size));
    return size;
}

static ZR_FORCE_INLINE TZrFloat64 io_read_float(SZrIo *io) {
    TZrFloat64 value;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TZrInt64 io_read_int(SZrIo *io) {
    TZrInt64 value;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TZrUInt64 io_read_u_int(SZrIo *io) {
    TZrUInt64 value;
    ZrCore_Io_Read(io, (TZrBytePtr) &value, sizeof(value));
    return value;
}

#define ZR_IO_READ_RAW(IO, VALUE, SIZE) ZrCore_Io_Read(IO, &(VALUE), SIZE);

#define ZR_IO_READ_NATIVE_TYPE(IO, DATA, TYPE) ZrCore_Io_Read(IO, (TZrBytePtr) & (DATA), sizeof(TYPE))


static SZrString *io_read_string_with_length(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    TZrSize length = io_read_size(io);
    if (length == 0) {
        return ZR_NULL;
    }
    TZrNativeString nativeString = ZrCore_Memory_RawMallocWithType(global, length + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    ZrCore_Io_Read(io, (TZrBytePtr) nativeString, length);
    nativeString[length] = '\0';
    SZrString *string = ZrCore_String_Create(io->state, nativeString, length);
    ZrCore_Memory_RawFreeWithType(global, nativeString, length + 1, ZR_MEMORY_NATIVE_TYPE_NATIVE_STRING);
    return string;
}

static void io_read_imports(SZrIo *io, SZrIoImport *imports, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoImport *import = &imports[i];
        import->name = io_read_string_with_length(io);
        import->md5 = io_read_string_with_length(io);
    }
}

static void io_read_typed_value(SZrIo *io, SZrTypeValue *value);

static void io_init_value_from_pure(struct SZrState *state,
                                    EZrValueType type,
                                    const TZrPureValue *pure,
                                    SZrTypeValue *value) {
    if (value == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(value);
    if (pure == ZR_NULL) {
        return;
    }

    switch (type) {
        case ZR_VALUE_TYPE_NULL:
            return;

        case ZR_VALUE_TYPE_BOOL:
            ZrCore_Value_InitAsBool(state, value, pure->nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
            return;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            ZrCore_Value_InitAsInt(state, value, pure->nativeObject.nativeInt64);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            ZrCore_Value_InitAsUInt(state, value, pure->nativeObject.nativeUInt64);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            ZrCore_Value_InitAsFloat(state, value, pure->nativeObject.nativeDouble);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            ZrCore_Value_InitAsRawObject(state, value, pure->object);
            value->type = type;
            return;

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            ZrCore_Value_InitAsNativePointer(state, value, pure->nativeObject.nativePointer);
            return;

        default:
            value->type = type;
            value->value = *pure;
            value->isGarbageCollectable =
                    (TZrBool)(type == ZR_VALUE_TYPE_STRING || type == ZR_VALUE_TYPE_OBJECT || type == ZR_VALUE_TYPE_ARRAY);
            value->isNative = (TZrBool)!value->isGarbageCollectable;
            return;
    }
}

static void io_read_value(SZrIo *io, EZrValueType type, TZrPureValue *value) {
    SZrState *state;

    if (io == ZR_NULL || value == ZR_NULL) {
        return;
    }

    state = io->state;
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            value->nativeObject.nativeBool = ZR_FALSE;
        } break;
        case ZR_VALUE_TYPE_BOOL: {
            value->nativeObject.nativeBool = io_read_char(io);
        } break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            value->nativeObject.nativeDouble = io_read_float(io);
        } break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            value->nativeObject.nativeInt64 = io_read_int(io);
        } break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            value->nativeObject.nativeUInt64 = io_read_u_int(io);
        } break;
        case ZR_VALUE_TYPE_STRING: {
            SZrString *stringValue = io_read_string_with_length(io);
            if (stringValue == ZR_NULL) {
                stringValue = ZrCore_String_Create(state, "", 0);
            }
            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue);
        } break;
        case ZR_VALUE_TYPE_OBJECT: {
            TZrSize entryCount = 0;
            SZrObject *object;

            ZR_IO_READ_NATIVE_TYPE(io, entryCount, TZrSize);
            object = ZrCore_Object_New(state, ZR_NULL);
            if (object == ZR_NULL) {
                value->object = ZR_NULL;
                break;
            }
            ZrCore_Object_Init(state, object);

            for (TZrSize index = 0; index < entryCount; index++) {
                SZrTypeValue keyValue;
                SZrTypeValue entryValue;

                io_read_typed_value(io, &keyValue);
                io_read_typed_value(io, &entryValue);
                ZrCore_Object_SetValue(state, object, &keyValue, &entryValue);
            }

            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(object);
        } break;
        case ZR_VALUE_TYPE_ARRAY: {
            TZrSize elementCount = 0;
            SZrObject *arrayObject;
            SZrTypeValue receiver;

            ZR_IO_READ_NATIVE_TYPE(io, elementCount, TZrSize);
            arrayObject = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
            if (arrayObject == ZR_NULL) {
                value->object = ZR_NULL;
                break;
            }
            ZrCore_Object_Init(state, arrayObject);

            ZrCore_Value_InitAsRawObject(state, &receiver, ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject));
            receiver.type = ZR_VALUE_TYPE_ARRAY;
            for (TZrSize index = 0; index < elementCount; index++) {
                SZrTypeValue indexValue;
                SZrTypeValue elementValue;

                io_read_typed_value(io, &elementValue);
                ZrCore_Value_InitAsInt(state, &indexValue, (TZrInt64)index);
                ZrCore_Object_SetByIndex(state, &receiver, &indexValue, &elementValue);
            }

            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(arrayObject);
        } break;
        default: {
            // todo:
        } break;
    }
}

static void io_read_typed_value(SZrIo *io, SZrTypeValue *value) {
    TZrUInt32 rawType = ZR_VALUE_TYPE_NULL;
    TZrPureValue pureValue;

    if (value == ZR_NULL) {
        return;
    }

    ZrCore_Value_ResetAsNull(value);
    if (io == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&pureValue, 0, sizeof(pureValue));
    ZR_IO_READ_NATIVE_TYPE(io, rawType, TZrUInt32);
    io_read_value(io, (EZrValueType)rawType, &pureValue);
    io_init_value_from_pure(io->state, (EZrValueType)rawType, &pureValue, value);
}

static void io_read_field(SZrIo *io, SZrIoField *field) { field->name = io_read_string_with_length(io); }

static void io_read_references(SZrIo *io, SZrIoReference *references, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoReference *reference = &references[i];
        reference->referenceModuleName = io_read_string_with_length(io);
        reference->referenceModuleMd5 = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, reference->referenceIndex, TZrSize);
    }
}

static void io_read_function_local_variables(SZrIo *io, SZrIoFunctionLocalVariable *variables, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionLocalVariable *variable = &variables[i];
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionStartIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionEndIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TZrUInt64);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->scopeDepth, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, variable->escapeFlags, TZrUInt32);
        } else {
            variable->scopeDepth = 0;
            variable->escapeFlags = 0;
        }
    }
}

static void io_read_function_closure_variables(SZrIo *io, SZrIoFunctionClosureVariable *variables, TZrSize count) {
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionClosureVariable *variable = &variables[i];
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->inStack, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->index, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->valueType, TZrUInt32);
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->scopeDepth, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, variable->escapeFlags, TZrUInt32);
        } else {
            variable->scopeDepth = 0;
            variable->escapeFlags = 0;
        }
    }
}

static void io_read_function_catch_clauses(SZrIo *io, SZrIoFunctionCatchClause *clauses, TZrSize count) {
    for (TZrSize i = 0; i < count; i++) {
        clauses[i].typeName = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, clauses[i].targetInstructionOffset, TZrUInt64);
    }
}

static void io_read_function_exception_handlers(SZrIo *io,
                                                SZrIoFunctionExceptionHandler *handlers,
                                                TZrSize count) {
    for (TZrSize i = 0; i < count; i++) {
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].protectedStartInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].finallyTargetInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].afterFinallyInstructionOffset, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].catchClauseStartIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].catchClauseCount, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, handlers[i].hasFinally, TZrUInt8);
    }
}

static void io_read_functions(SZrIo *io, SZrIoFunction *functions, TZrSize count);
static void io_read_function_constant_variables(SZrIo *io, SZrIoFunctionConstantVariable *variables, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionConstantVariable *variable = &variables[i];
        ZR_IO_READ_NATIVE_TYPE(io, variable->type, TZrUInt32);
        variable->hasFunctionValue = ZR_FALSE;
        variable->functionValue = ZR_NULL;
        if (variable->type == ZR_VALUE_TYPE_FUNCTION || variable->type == ZR_VALUE_TYPE_CLOSURE) {
            ZR_IO_READ_NATIVE_TYPE(io, variable->hasFunctionValue, TZrBool);
            if (variable->hasFunctionValue) {
                variable->functionValue = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
                io_read_functions(io, variable->functionValue, 1);
            }
        } else {
            io_read_value(io, variable->type, &variable->value);
        }
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TZrUInt64);
    }
}

static void io_read_function_exported_variables(SZrIo *io, SZrIoFunctionExportedVariable *variables, TZrSize count) {
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionExportedVariable *variable = &variables[i];
        ZrCore_Memory_RawSet(variable, 0, sizeof(*variable));
        variable->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, variable->callableChildIndex, TZrUInt32);
    }
}

static void io_read_function_typed_type_ref(SZrIo *io, SZrIoFunctionTypedTypeRef *typeRef) {
    TZrUInt32 baseType = ZR_VALUE_TYPE_OBJECT;
    TZrUInt8 isNullable = ZR_FALSE;
    TZrUInt32 ownershipQualifier = 0;
    TZrUInt8 isArray = ZR_FALSE;
    TZrUInt32 elementBaseType = ZR_VALUE_TYPE_OBJECT;

    if (io == ZR_NULL || typeRef == ZR_NULL) {
        return;
    }

    ZR_IO_READ_NATIVE_TYPE(io, baseType, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, isNullable, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, ownershipQualifier, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, isArray, TZrUInt8);
    typeRef->typeName = io_read_string_with_length(io);
    ZR_IO_READ_NATIVE_TYPE(io, elementBaseType, TZrUInt32);
    typeRef->elementTypeName = io_read_string_with_length(io);

    typeRef->baseType = (EZrValueType)baseType;
    typeRef->isNullable = isNullable ? ZR_TRUE : ZR_FALSE;
    typeRef->ownershipQualifier = ownershipQualifier;
    typeRef->isArray = isArray ? ZR_TRUE : ZR_FALSE;
    typeRef->elementBaseType = (EZrValueType)elementBaseType;
}

static void io_read_function_typed_local_bindings(SZrIo *io,
                                                  SZrIoFunctionTypedLocalBinding *bindings,
                                                  TZrSize count) {
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionTypedLocalBinding *binding = &bindings[i];
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->stackSlot, TZrUInt32);
        io_read_function_typed_type_ref(io, &binding->type);
    }
}

static void io_read_function_typed_export_symbols(SZrIo *io,
                                                  SZrIoFunctionTypedExportSymbol *symbols,
                                                  TZrSize count) {
    SZrGlobalState *global = io->state->global;

    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionTypedExportSymbol *symbol = &symbols[i];
        ZrCore_Memory_RawSet(symbol, 0, sizeof(*symbol));
        symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;

        symbol->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->symbolKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->reserved0, TZrUInt16);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->callableChildIndex, TZrUInt32);
        io_read_function_typed_type_ref(io, &symbol->valueType);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->parameterCount, TZrSize);
        if (symbol->parameterCount > 0) {
            symbol->parameterTypes = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                              sizeof(SZrIoFunctionTypedTypeRef) *
                                                                      symbol->parameterCount);
            if (symbol->parameterTypes != ZR_NULL) {
                for (TZrSize paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
                    io_read_function_typed_type_ref(io, &symbol->parameterTypes[paramIndex]);
                }
            }
        } else {
            symbol->parameterTypes = ZR_NULL;
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_TYPED_EXPORT_DECLARATION_SPANS) {
            ZR_IO_READ_NATIVE_TYPE(io, symbol->lineInSourceStart, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->columnInSourceStart, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->lineInSourceEnd, TZrUInt32);
            ZR_IO_READ_NATIVE_TYPE(io, symbol->columnInSourceEnd, TZrUInt32);
        } else {
            symbol->lineInSourceStart = 0;
            symbol->columnInSourceStart = 0;
            symbol->lineInSourceEnd = 0;
            symbol->columnInSourceEnd = 0;
        }
    }
}

static void io_read_function_module_effects(SZrIo *io,
                                            SZrIoFunctionModuleEffect *effects,
                                            TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        SZrIoFunctionModuleEffect *effect = &effects[index];

        ZrCore_Memory_RawSet(effect, 0, sizeof(*effect));
        ZR_IO_READ_NATIVE_TYPE(io, effect->kind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, effect->reserved0, TZrUInt8);
        effect->moduleName = io_read_string_with_length(io);
        effect->symbolName = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, effect->lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->columnInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->lineInSourceEnd, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effect->columnInSourceEnd, TZrUInt32);
    }
}

static void io_read_function_callable_summaries(SZrIo *io,
                                                SZrIoFunctionCallableSummary *summaries,
                                                TZrSize count) {
    SZrGlobalState *global = io->state->global;

    for (TZrSize index = 0; index < count; index++) {
        SZrIoFunctionCallableSummary *summary = &summaries[index];

        ZrCore_Memory_RawSet(summary, 0, sizeof(*summary));
        summary->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, summary->callableChildIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, summary->effectCount, TZrSize);
        if (summary->effectCount > 0) {
            summary->effects = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                        sizeof(SZrIoFunctionModuleEffect) * summary->effectCount);
            if (summary->effects != ZR_NULL) {
                io_read_function_module_effects(io, summary->effects, summary->effectCount);
            }
        }
    }
}

static void io_read_function_top_level_callable_bindings(SZrIo *io,
                                                         SZrIoFunctionTopLevelCallableBinding *bindings,
                                                         TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        SZrIoFunctionTopLevelCallableBinding *binding = &bindings[index];

        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        binding->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->callableChildIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->exportKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->readiness, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved0, TZrUInt8);
    }
}

static void io_read_function_metadata_parameters(SZrIo *io,
                                                 SZrIoFunctionMetadataParameter **outParameters,
                                                 TZrSize *outCount) {
    SZrGlobalState *global;
    TZrSize count;

    if (io == ZR_NULL || outParameters == ZR_NULL || outCount == ZR_NULL) {
        return;
    }

    global = io->state->global;
    ZR_IO_READ_NATIVE_TYPE(io, count, TZrSize);
    *outCount = count;
    *outParameters = ZR_NULL;
    if (count == 0) {
        return;
    }

    *outParameters = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionMetadataParameter) * count);
    if (*outParameters == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < count; index++) {
        (*outParameters)[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &(*outParameters)[index].type);
        (*outParameters)[index].hasDefaultValue = ZR_FALSE;
        ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].hasDefaultValue, TZrUInt8);
        if ((*outParameters)[index].hasDefaultValue) {
            io_read_function_constant_variables(io, &(*outParameters)[index].defaultValue, 1);
        } else {
            ZrCore_Memory_RawSet(&(*outParameters)[index].defaultValue, 0, sizeof((*outParameters)[index].defaultValue));
        }
        (*outParameters)[index].hasDecoratorMetadata = ZR_FALSE;
        ZrCore_Memory_RawSet(&(*outParameters)[index].decoratorMetadataValue,
                             0,
                             sizeof((*outParameters)[index].decoratorMetadataValue));
        (*outParameters)[index].decoratorNamesLength = 0;
        (*outParameters)[index].decoratorNames = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].hasDecoratorMetadata, TZrUInt8);
            if ((*outParameters)[index].hasDecoratorMetadata) {
                io_read_function_constant_variables(io, &(*outParameters)[index].decoratorMetadataValue, 1);
            }

            ZR_IO_READ_NATIVE_TYPE(io, (*outParameters)[index].decoratorNamesLength, TZrSize);
            if ((*outParameters)[index].decoratorNamesLength > 0) {
                (*outParameters)[index].decoratorNames =
                        ZR_IO_MALLOC_NATIVE_DATA(global,
                                                 sizeof(SZrString *) * (*outParameters)[index].decoratorNamesLength);
                if ((*outParameters)[index].decoratorNames != ZR_NULL) {
                    for (TZrSize decoratorIndex = 0;
                         decoratorIndex < (*outParameters)[index].decoratorNamesLength;
                         decoratorIndex++) {
                        (*outParameters)[index].decoratorNames[decoratorIndex] = io_read_string_with_length(io);
                    }
                }
            }
        }
    }
}

static void io_read_function_compile_time_variable_infos(SZrIo *io,
                                                         SZrIoFunctionCompileTimeVariableInfo *infos,
                                                         TZrSize count) {
    SZrGlobalState *global = io != ZR_NULL && io->state != ZR_NULL ? io->state->global : ZR_NULL;

    for (TZrSize index = 0; index < count; index++) {
        infos[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &infos[index].type);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceEnd, TZrUInt32);
        infos[index].pathBindingsLength = 0;
        infos[index].pathBindings = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_PATH_BINDINGS) {
            ZR_IO_READ_NATIVE_TYPE(io, infos[index].pathBindingsLength, TZrSize);
            if (infos[index].pathBindingsLength > 0) {
                if (global != ZR_NULL) {
                    infos[index].pathBindings = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                         sizeof(SZrIoFunctionCompileTimePathBinding) *
                                                                                 infos[index].pathBindingsLength);
                }
                for (TZrSize bindingIndex = 0; bindingIndex < infos[index].pathBindingsLength; bindingIndex++) {
                    SZrString *path = io_read_string_with_length(io);
                    TZrUInt8 targetKind = 0;
                    SZrString *targetName;

                    ZR_IO_READ_NATIVE_TYPE(io, targetKind, TZrUInt8);
                    targetName = io_read_string_with_length(io);
                    if (infos[index].pathBindings != ZR_NULL) {
                        infos[index].pathBindings[bindingIndex].path = path;
                        infos[index].pathBindings[bindingIndex].targetKind = targetKind;
                        infos[index].pathBindings[bindingIndex].targetName = targetName;
                    }
                }
            }
        }
    }
}

static void io_read_function_compile_time_function_infos(SZrIo *io,
                                                         SZrIoFunctionCompileTimeFunctionInfo *infos,
                                                         TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        infos[index].name = io_read_string_with_length(io);
        io_read_function_typed_type_ref(io, &infos[index].returnType);
        io_read_function_metadata_parameters(io, &infos[index].parameters, &infos[index].parameterCount);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceEnd, TZrUInt32);
    }
}

static void io_read_function_escape_bindings(SZrIo *io,
                                             SZrIoFunctionEscapeBinding *bindings,
                                             TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        SZrIoFunctionEscapeBinding *binding = &bindings[index];

        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
        binding->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, binding->slotOrIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->scopeDepth, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->escapeFlags, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, binding->bindingKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, binding->reserved1, TZrUInt16);
    }
}

static void io_read_function_test_infos(SZrIo *io,
                                        SZrIoFunctionTestInfo *infos,
                                        TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        TZrUInt8 hasVariableArguments = ZR_FALSE;

        infos[index].name = io_read_string_with_length(io);
        io_read_function_metadata_parameters(io, &infos[index].parameters, &infos[index].parameterCount);
        ZR_IO_READ_NATIVE_TYPE(io, hasVariableArguments, TZrUInt8);
        infos[index].hasVariableArguments = hasVariableArguments ? ZR_TRUE : ZR_FALSE;
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceStart, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, infos[index].lineInSourceEnd, TZrUInt32);
    }
}

static void io_read_function_decorator_metadata(SZrIo *io, SZrIoFunction *function) {
    SZrGlobalState *global;

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    global = io->state->global;
    function->hasDecoratorMetadata = ZR_FALSE;
    function->decoratorMetadataValue.type = ZR_VALUE_TYPE_NULL;
    ZrCore_Memory_RawSet(&function->decoratorMetadataValue.value, 0, sizeof(function->decoratorMetadataValue.value));
    function->decoratorMetadataValue.hasFunctionValue = ZR_FALSE;
    function->decoratorMetadataValue.functionValue = ZR_NULL;
    function->decoratorMetadataValue.startLine = 0;
    function->decoratorMetadataValue.endLine = 0;
    function->decoratorNamesLength = 0;
    function->decoratorNames = ZR_NULL;

    ZR_IO_READ_NATIVE_TYPE(io, function->hasDecoratorMetadata, TZrUInt8);
    if (function->hasDecoratorMetadata) {
        io_read_function_constant_variables(io, &function->decoratorMetadataValue, 1);
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->decoratorNamesLength, TZrSize);
    if (function->decoratorNamesLength > 0) {
        function->decoratorNames =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrString *) * function->decoratorNamesLength);
        if (function->decoratorNames != ZR_NULL) {
            for (TZrSize index = 0; index < function->decoratorNamesLength; index++) {
                function->decoratorNames[index] = io_read_string_with_length(io);
            }
        }
    }
}

static void io_read_function_member_entries(SZrIo *io,
                                            SZrIoFunctionMemberEntry *entries,
                                            TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        entries[index].symbol = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].entryKind, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].reserved0, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].reserved1, TZrUInt16);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].prototypeIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, entries[index].descriptorIndex, TZrUInt32);
    }
}

static void io_read_function_semir_type_table(SZrIo *io,
                                              SZrIoFunctionTypedTypeRef *typeTable,
                                              TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        io_read_function_typed_type_ref(io, &typeTable[index]);
    }
}

static void io_read_function_semir_ownership_table(SZrIo *io,
                                                   SZrIoSemIrOwnershipEntry *ownershipTable,
                                                   TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, ownershipTable[index].state, TZrUInt32);
    }
}

static void io_read_function_semir_effect_table(SZrIo *io,
                                                SZrIoSemIrEffectEntry *effectTable,
                                                TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].kind, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].instructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].ownershipInputIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, effectTable[index].ownershipOutputIndex, TZrUInt32);
    }
}

static void io_read_function_semir_block_table(SZrIo *io,
                                               SZrIoSemIrBlockEntry *blockTable,
                                               TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].blockId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].firstInstructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, blockTable[index].instructionCount, TZrUInt32);
    }
}

static void io_read_function_semir_instruction_table(SZrIo *io,
                                                     SZrIoSemIrInstruction *instructionTable,
                                                     TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].opcode, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].execInstructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].typeTableIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].effectTableIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].destinationSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].operand0, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].operand1, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, instructionTable[index].deoptId, TZrUInt32);
    }
}

static void io_read_function_semir_deopt_table(SZrIo *io,
                                               SZrIoSemIrDeoptEntry *deoptTable,
                                               TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, deoptTable[index].deoptId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, deoptTable[index].execInstructionIndex, TZrUInt32);
    }
}

static void io_read_function_callsite_cache_table(SZrIo *io,
                                                  SZrIoFunctionCallSiteCacheEntry *cacheTable,
                                                  TZrSize count) {
    for (TZrSize index = 0; index < count; index++) {
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].kind, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].instructionIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].memberEntryIndex, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].deoptId, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, cacheTable[index].argumentCount, TZrUInt32);
    }
}

static void io_read_function_semir_metadata(SZrIo *io, SZrIoFunction *function) {
    SZrGlobalState *global;

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    global = io->state->global;
    ZR_IO_READ_NATIVE_TYPE(io, function->semIrTypeTableLength, TZrSize);
    if (function->semIrTypeTableLength > 0) {
        function->semIrTypeTable = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                            sizeof(SZrIoFunctionTypedTypeRef) *
                                                                    function->semIrTypeTableLength);
        if (function->semIrTypeTable != ZR_NULL) {
            io_read_function_semir_type_table(io, function->semIrTypeTable, function->semIrTypeTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrOwnershipTableLength, TZrSize);
    if (function->semIrOwnershipTableLength > 0) {
        function->semIrOwnershipTable = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                 sizeof(SZrIoSemIrOwnershipEntry) *
                                                                         function->semIrOwnershipTableLength);
        if (function->semIrOwnershipTable != ZR_NULL) {
            io_read_function_semir_ownership_table(io,
                                                   function->semIrOwnershipTable,
                                                   function->semIrOwnershipTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrEffectTableLength, TZrSize);
    if (function->semIrEffectTableLength > 0) {
        function->semIrEffectTable = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                              sizeof(SZrIoSemIrEffectEntry) *
                                                                      function->semIrEffectTableLength);
        if (function->semIrEffectTable != ZR_NULL) {
            io_read_function_semir_effect_table(io, function->semIrEffectTable, function->semIrEffectTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrBlockTableLength, TZrSize);
    if (function->semIrBlockTableLength > 0) {
        function->semIrBlockTable = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                             sizeof(SZrIoSemIrBlockEntry) *
                                                                     function->semIrBlockTableLength);
        if (function->semIrBlockTable != ZR_NULL) {
            io_read_function_semir_block_table(io, function->semIrBlockTable, function->semIrBlockTableLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrInstructionLength, TZrSize);
    if (function->semIrInstructionLength > 0) {
        function->semIrInstructions = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                               sizeof(SZrIoSemIrInstruction) *
                                                                       function->semIrInstructionLength);
        if (function->semIrInstructions != ZR_NULL) {
            io_read_function_semir_instruction_table(io,
                                                     function->semIrInstructions,
                                                     function->semIrInstructionLength);
        }
    }

    ZR_IO_READ_NATIVE_TYPE(io, function->semIrDeoptTableLength, TZrSize);
    if (function->semIrDeoptTableLength > 0) {
        function->semIrDeoptTable = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                             sizeof(SZrIoSemIrDeoptEntry) *
                                                                     function->semIrDeoptTableLength);
        if (function->semIrDeoptTable != ZR_NULL) {
            io_read_function_semir_deopt_table(io, function->semIrDeoptTable, function->semIrDeoptTableLength);
        }
    }
}

static void io_read_function_callsite_cache_metadata(SZrIo *io, SZrIoFunction *function) {
    SZrGlobalState *global;

    if (io == ZR_NULL || function == ZR_NULL || io->state == ZR_NULL || io->state->global == ZR_NULL) {
        return;
    }

    global = io->state->global;
    ZR_IO_READ_NATIVE_TYPE(io, function->callSiteCacheLength, TZrSize);
    if (function->callSiteCacheLength > 0) {
        function->callSiteCaches = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                            sizeof(SZrIoFunctionCallSiteCacheEntry) *
                                                                    function->callSiteCacheLength);
        if (function->callSiteCaches != ZR_NULL) {
            io_read_function_callsite_cache_table(io, function->callSiteCaches, function->callSiteCacheLength);
        }
    }
}

static void io_read_classes(SZrIo *io, SZrIoClass *classes, TZrSize count);
static void io_read_structs(SZrIo *io, SZrIoStruct *structs, TZrSize count);

static void io_read_function_closures(SZrIo *io, SZrIoFunctionClosure *closures, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionClosure *closure = &closures[i];
        closure->subFunction = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
        io_read_functions(io, closure->subFunction, 1);
        // todo:
    }
}

static void io_read_function_debug_infos(SZrIo *io, SZrIoFunctionDebugInfo *debugInfos, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionDebugInfo *debugInfo = &debugInfos[i];
        debugInfo->sourceFile = ZR_NULL;
        debugInfo->sourceHash = ZR_NULL;
        debugInfo->instructionRanges = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_IDENTITY) {
            debugInfo->sourceFile = io_read_string_with_length(io);
            debugInfo->sourceHash = io_read_string_with_length(io);
        }
        ZR_IO_READ_NATIVE_TYPE(io, debugInfo->instructionsLength, TZrSize);
        debugInfo->instructionsLine = ZR_NULL;
        if (debugInfo->instructionsLength > 0) {
            debugInfo->instructionsLine =
                    ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TZrUInt64) * debugInfo->instructionsLength);
            if (debugInfo->instructionsLine != ZR_NULL) {
                ZrCore_Io_Read(io,
                               (TZrBytePtr) debugInfo->instructionsLine,
                               sizeof(TZrUInt64) * debugInfo->instructionsLength);
            } else {
                TZrUInt64 ignoredLine = 0;
                for (TZrSize index = 0; index < debugInfo->instructionsLength; index++) {
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredLine, TZrUInt64);
                }
            }
        }

        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_SOURCE_RANGES &&
            debugInfo->instructionsLength > 0) {
            debugInfo->instructionRanges = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                    sizeof(SZrIoInstructionSourceRange) *
                                                                            debugInfo->instructionsLength);
            if (debugInfo->instructionRanges != ZR_NULL) {
                ZrCore_Io_Read(io,
                               (TZrBytePtr) debugInfo->instructionRanges,
                               sizeof(SZrIoInstructionSourceRange) * debugInfo->instructionsLength);
            } else {
                SZrIoInstructionSourceRange ignoredRange;
                for (TZrSize index = 0; index < debugInfo->instructionsLength; index++) {
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.startLine, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.startColumn, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.endLine, TZrUInt32);
                    ZR_IO_READ_NATIVE_TYPE(io, ignoredRange.endColumn, TZrUInt32);
                }
            }
        }
    }
}


static void io_read_functions(SZrIo *io, SZrIoFunction *functions, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunction *function = &functions[i];
        function->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, function->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->endLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->parametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, function->hasVarArgs, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->stackSize, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, function->instructionsLength, TZrSize);
        // read instructions ...
        if (function->instructionsLength > 0) {
            function->instructions =
                    ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TZrInstruction) * function->instructionsLength);
            ZrCore_Io_Read(io, (TZrBytePtr) function->instructions, sizeof(TZrInstruction) * function->instructionsLength);
        } else {
            function->instructions = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->localVariablesLength, TZrSize);
        if (function->localVariablesLength > 0) {
            function->localVariables = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionLocalVariable) *
                                                                                function->localVariablesLength);
            io_read_function_local_variables(io, function->localVariables, function->localVariablesLength);
        } else {
            function->localVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->closureVariablesLength, TZrSize);
        if (function->closureVariablesLength > 0) {
            function->closureVariables = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionClosureVariable) *
                                                                                  function->closureVariablesLength);
            if (function->closureVariables != ZR_NULL) {
                io_read_function_closure_variables(io, function->closureVariables, function->closureVariablesLength);
            }
        } else {
            function->closureVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->catchClauseCount, TZrSize);
        if (function->catchClauseCount > 0) {
            function->catchClauses = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionCatchClause) *
                                                                      function->catchClauseCount);
            if (function->catchClauses != ZR_NULL) {
                io_read_function_catch_clauses(io, function->catchClauses, function->catchClauseCount);
            }
        } else {
            function->catchClauses = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exceptionHandlerCount, TZrSize);
        if (function->exceptionHandlerCount > 0) {
            function->exceptionHandlers = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionExceptionHandler) *
                                                                           function->exceptionHandlerCount);
            if (function->exceptionHandlers != ZR_NULL) {
                io_read_function_exception_handlers(io,
                                                    function->exceptionHandlers,
                                                    function->exceptionHandlerCount);
            }
        } else {
            function->exceptionHandlers = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->constantVariablesLength, TZrSize);
        if (function->constantVariablesLength > 0) {
            function->constantVariables = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionConstantVariable) *
                                                                                   function->constantVariablesLength);
            io_read_function_constant_variables(io, function->constantVariables, function->constantVariablesLength);
        } else {
            function->constantVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exportedVariablesLength, TZrSize);
        if (function->exportedVariablesLength > 0) {
            function->exportedVariables = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionExportedVariable) *
                                                                                   function->exportedVariablesLength);
            io_read_function_exported_variables(io, function->exportedVariables, function->exportedVariablesLength);
        } else {
            function->exportedVariables = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->typedLocalBindingsLength, TZrSize);
        if (function->typedLocalBindingsLength > 0) {
            function->typedLocalBindings = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                    sizeof(SZrIoFunctionTypedLocalBinding) *
                                                                            function->typedLocalBindingsLength);
            io_read_function_typed_local_bindings(io,
                                                  function->typedLocalBindings,
                                                  function->typedLocalBindingsLength);
        } else {
            function->typedLocalBindings = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->typedExportedSymbolsLength, TZrSize);
        if (function->typedExportedSymbolsLength > 0) {
            function->typedExportedSymbols = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                      sizeof(SZrIoFunctionTypedExportSymbol) *
                                                                              function->typedExportedSymbolsLength);
            io_read_function_typed_export_symbols(io,
                                                  function->typedExportedSymbols,
                                                  function->typedExportedSymbolsLength);
        } else {
            function->typedExportedSymbols = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->staticImportsLength, TZrSize);
        if (function->staticImportsLength > 0) {
            function->staticImports = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                               sizeof(SZrString *) * function->staticImportsLength);
            if (function->staticImports != ZR_NULL) {
                for (TZrSize index = 0; index < function->staticImportsLength; index++) {
                    function->staticImports[index] = io_read_string_with_length(io);
                }
            }
        } else {
            function->staticImports = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->moduleEntryEffectsLength, TZrSize);
        if (function->moduleEntryEffectsLength > 0) {
            function->moduleEntryEffects = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                    sizeof(SZrIoFunctionModuleEffect) *
                                                                            function->moduleEntryEffectsLength);
            if (function->moduleEntryEffects != ZR_NULL) {
                io_read_function_module_effects(io,
                                                function->moduleEntryEffects,
                                                function->moduleEntryEffectsLength);
            }
        } else {
            function->moduleEntryEffects = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->exportedCallableSummariesLength, TZrSize);
        if (function->exportedCallableSummariesLength > 0) {
            function->exportedCallableSummaries = ZR_IO_MALLOC_NATIVE_DATA(global,
                                                                           sizeof(SZrIoFunctionCallableSummary) *
                                                                                   function->exportedCallableSummariesLength);
            if (function->exportedCallableSummaries != ZR_NULL) {
                io_read_function_callable_summaries(io,
                                                    function->exportedCallableSummaries,
                                                    function->exportedCallableSummariesLength);
            }
        } else {
            function->exportedCallableSummaries = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->topLevelCallableBindingsLength, TZrSize);
        if (function->topLevelCallableBindingsLength > 0) {
            function->topLevelCallableBindings = ZR_IO_MALLOC_NATIVE_DATA(
                    global,
                    sizeof(SZrIoFunctionTopLevelCallableBinding) * function->topLevelCallableBindingsLength);
            if (function->topLevelCallableBindings != ZR_NULL) {
                io_read_function_top_level_callable_bindings(io,
                                                             function->topLevelCallableBindings,
                                                             function->topLevelCallableBindingsLength);
            }
        } else {
            function->topLevelCallableBindings = ZR_NULL;
        }
        function->parameterMetadataLength = 0;
        function->parameterMetadata = ZR_NULL;
        function->hasCallableReturnType = ZR_FALSE;
        ZrCore_Memory_RawSet(&function->callableReturnType, 0, sizeof(function->callableReturnType));
        function->callableReturnType.baseType = ZR_VALUE_TYPE_OBJECT;
        function->callableReturnType.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        function->compileTimeVariableInfosLength = 0;
        function->compileTimeVariableInfos = ZR_NULL;
        function->compileTimeFunctionInfosLength = 0;
        function->compileTimeFunctionInfos = ZR_NULL;
        function->escapeBindingLength = 0;
        function->escapeBindings = ZR_NULL;
        function->returnEscapeSlotCount = 0;
        function->returnEscapeSlots = ZR_NULL;
        function->testInfosLength = 0;
        function->testInfos = ZR_NULL;
        function->hasDecoratorMetadata = ZR_FALSE;
        function->decoratorMetadataValue.type = ZR_VALUE_TYPE_NULL;
        ZrCore_Memory_RawSet(&function->decoratorMetadataValue.value, 0, sizeof(function->decoratorMetadataValue.value));
        function->decoratorMetadataValue.hasFunctionValue = ZR_FALSE;
        function->decoratorMetadataValue.functionValue = ZR_NULL;
        function->decoratorMetadataValue.startLine = 0;
        function->decoratorMetadataValue.endLine = 0;
        function->decoratorNamesLength = 0;
        function->decoratorNames = ZR_NULL;
        function->memberEntriesLength = 0;
        function->memberEntries = ZR_NULL;
        function->semIrTypeTableLength = 0;
        function->semIrTypeTable = ZR_NULL;
        function->semIrOwnershipTableLength = 0;
        function->semIrOwnershipTable = ZR_NULL;
        function->semIrEffectTableLength = 0;
        function->semIrEffectTable = ZR_NULL;
        function->semIrBlockTableLength = 0;
        function->semIrBlockTable = ZR_NULL;
        function->semIrInstructionLength = 0;
        function->semIrInstructions = ZR_NULL;
        function->semIrDeoptTableLength = 0;
        function->semIrDeoptTable = ZR_NULL;
        function->callSiteCacheLength = 0;
        function->callSiteCaches = ZR_NULL;
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_PARAMETER_METADATA) {
            io_read_function_metadata_parameters(io, &function->parameterMetadata, &function->parameterMetadataLength);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_CALLABLE_RETURN_TYPE) {
            ZR_IO_READ_NATIVE_TYPE(io, function->hasCallableReturnType, TZrUInt8);
            if (function->hasCallableReturnType) {
                io_read_function_typed_type_ref(io, &function->callableReturnType);
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA) {
            ZR_IO_READ_NATIVE_TYPE(io, function->compileTimeVariableInfosLength, TZrSize);
            if (function->compileTimeVariableInfosLength > 0) {
                function->compileTimeVariableInfos =
                        ZR_IO_MALLOC_NATIVE_DATA(global,
                                                 sizeof(SZrIoFunctionCompileTimeVariableInfo) *
                                                         function->compileTimeVariableInfosLength);
                if (function->compileTimeVariableInfos != ZR_NULL) {
                    io_read_function_compile_time_variable_infos(io,
                                                                 function->compileTimeVariableInfos,
                                                                 function->compileTimeVariableInfosLength);
                }
            }
            ZR_IO_READ_NATIVE_TYPE(io, function->compileTimeFunctionInfosLength, TZrSize);
            if (function->compileTimeFunctionInfosLength > 0) {
                function->compileTimeFunctionInfos =
                        ZR_IO_MALLOC_NATIVE_DATA(global,
                                                 sizeof(SZrIoFunctionCompileTimeFunctionInfo) *
                                                         function->compileTimeFunctionInfosLength);
                if (function->compileTimeFunctionInfos != ZR_NULL) {
                    io_read_function_compile_time_function_infos(io,
                                                                 function->compileTimeFunctionInfos,
                                                                 function->compileTimeFunctionInfosLength);
                }
            }
            if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_ESCAPE_METADATA) {
                ZR_IO_READ_NATIVE_TYPE(io, function->escapeBindingLength, TZrSize);
                if (function->escapeBindingLength > 0) {
                    function->escapeBindings =
                            ZR_IO_MALLOC_NATIVE_DATA(global,
                                                     sizeof(SZrIoFunctionEscapeBinding) *
                                                             function->escapeBindingLength);
                    if (function->escapeBindings != ZR_NULL) {
                        io_read_function_escape_bindings(io,
                                                         function->escapeBindings,
                                                         function->escapeBindingLength);
                    }
                }
                ZR_IO_READ_NATIVE_TYPE(io, function->returnEscapeSlotCount, TZrSize);
                if (function->returnEscapeSlotCount > 0) {
                    function->returnEscapeSlots =
                            ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TZrUInt32) * function->returnEscapeSlotCount);
                    if (function->returnEscapeSlots != ZR_NULL) {
                        ZrCore_Io_Read(io,
                                       (TZrBytePtr)function->returnEscapeSlots,
                                       sizeof(TZrUInt32) * function->returnEscapeSlotCount);
                    }
                }
            }
            ZR_IO_READ_NATIVE_TYPE(io, function->testInfosLength, TZrSize);
            if (function->testInfosLength > 0) {
                function->testInfos =
                        ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionTestInfo) * function->testInfosLength);
                if (function->testInfos != ZR_NULL) {
                    io_read_function_test_infos(io, function->testInfos, function->testInfosLength);
                }
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_FUNCTION_DECORATOR_METADATA) {
            io_read_function_decorator_metadata(io, function);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES) {
            ZR_IO_READ_NATIVE_TYPE(io, function->memberEntriesLength, TZrSize);
            if (function->memberEntriesLength > 0) {
                function->memberEntries =
                        ZR_IO_MALLOC_NATIVE_DATA(global,
                                                 sizeof(SZrIoFunctionMemberEntry) * function->memberEntriesLength);
                if (function->memberEntries != ZR_NULL) {
                    io_read_function_member_entries(io, function->memberEntries, function->memberEntriesLength);
                }
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA) {
            io_read_function_semir_metadata(io, function);
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE) {
            io_read_function_callsite_cache_metadata(io, function);
        }
        // 读取PROTOTYPES_LENGTH和PROTOTYPES（结构化格式）
        ZR_IO_READ_NATIVE_TYPE(io, function->prototypesLength, TZrSize);
        function->classes = ZR_NULL;
        function->structs = ZR_NULL;
        function->prototypeDataLength = 0;
        function->prototypeData = ZR_NULL;

        // writer 总是会在 PROTOTYPES_LENGTH 后继续写入 CLASS/STRUCT count，
        // 即便 prototypesLength == 0 也会写两个 0，用于固定 .zro 布局。
        TZrSize classCount = 0;
        TZrSize structCount = 0;
        ZR_IO_READ_NATIVE_TYPE(io, classCount, TZrSize);
        function->classesLength = classCount;

        if (classCount > 0) {
            function->classes = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoClass) * classCount);
            if (function->classes != ZR_NULL) {
                io_read_classes(io, function->classes, classCount);
            }
        }

        ZR_IO_READ_NATIVE_TYPE(io, structCount, TZrSize);
        function->structsLength = structCount;
        if (structCount > 0) {
            function->structs = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoStruct) * structCount);
            if (function->structs != ZR_NULL) {
                io_read_structs(io, function->structs, structCount);
            }
        }
        if (io->sourceVersionPatch >= ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB) {
            ZR_IO_READ_NATIVE_TYPE(io, function->prototypeDataLength, TZrSize);
            if (function->prototypeDataLength > 0) {
                function->prototypeData = ZR_IO_MALLOC_NATIVE_DATA(global, function->prototypeDataLength);
                if (function->prototypeData != ZR_NULL) {
                    ZrCore_Io_Read(io, function->prototypeData, function->prototypeDataLength);
                }
            }
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->closuresLength, TZrSize);
        if (function->closuresLength > 0) {
            function->closures = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionClosure) *
                                                                          function->closuresLength);
            io_read_function_closures(io, function->closures, function->closuresLength);
        } else {
            function->closures = ZR_NULL;
        }
        ZR_IO_READ_NATIVE_TYPE(io, function->debugInfosLength, TZrSize);
        if (function->debugInfosLength > 0) {
            function->debugInfos =
                    ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionDebugInfo) * function->debugInfosLength);
            io_read_function_debug_infos(io, function->debugInfos, function->debugInfosLength);
        } else {
            function->debugInfos = ZR_NULL;
        }
    }
}

static void io_read_method(SZrIo *io, SZrIoMethod *method) {
    SZrGlobalState *global = io->state->global;
    method->name = io_read_string_with_length(io);
    ZR_IO_READ_NATIVE_TYPE(io, method->functionsLength, TZrSize);
    method->functions = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction) * method->functionsLength);
    io_read_functions(io, method->functions, method->functionsLength);
}


static void io_read_property(SZrIo *io, SZrIoProperty *property) {
    SZrGlobalState *global = io->state->global;
    property->name = io_read_string_with_length(io);
    // todo: use enum
    ZR_IO_READ_NATIVE_TYPE(io, property->propertyType, TZrUInt32);
    property->getter = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
    io_read_functions(io, property->getter, 1);
    property->setter = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
    io_read_functions(io, property->setter, 1);
}

static void io_read_meta(SZrIo *io, SZrIoMeta *meta) {
    SZrGlobalState *global = io->state->global;
    // todo: use enum
    ZR_IO_READ_NATIVE_TYPE(io, meta->metaType, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, meta->functionsLength, TZrSize);
    meta->functions = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction) * meta->functionsLength);
    io_read_functions(io, meta->functions, meta->functionsLength);
}

static void io_read_enum_fields(SZrIo *io, SZrIoEnumField *fields, TZrSize count, EZrValueType valueType) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoEnumField *field = &fields[i];
        field->name = io_read_string_with_length(io);
        io_read_value(io, valueType, &field->value);
    }
}

static void io_read_member_declares(SZrIo *io, SZrIoMemberDeclare *declares, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoMemberDeclare *declare = &declares[i];
        // todo:
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        switch (declare->type) {
            case ZR_IO_MEMBER_DECLARE_TYPE_METHOD: {
                // io_read_functions(io, declare->function, 1);
                declare->method = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMethod));
                io_read_method(io, declare->method);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_META: {
                declare->meta = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMeta));
                io_read_meta(io, declare->meta);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY: {
                declare->property = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoProperty));
                io_read_property(io, declare->property);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_ENUM: {
                declare->enumField = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoEnumField));
                io_read_enum_fields(io, declare->enumField, 1, ZR_VALUE_TYPE_UINT64);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_FIELD: {
                declare->field = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField));
                io_read_field(io, declare->field);
            } break;
            default:
                break;
        }
        // check type and read
    }
}

static void io_read_classes(SZrIo *io, SZrIoClass *classes, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoClass *class = &classes[i];
        class->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, class->superClassLength, TZrSize);
        class->superClasses = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * class->superClassLength);
        io_read_references(io, class->superClasses, class->superClassLength);
        ZR_IO_READ_NATIVE_TYPE(io, class->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, class->declaresLength, TZrSize);
        class->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * class->declaresLength);
        io_read_member_declares(io, class->declares, class->declaresLength);
    }
}

static void io_read_structs(SZrIo *io, SZrIoStruct *structs, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoStruct *struct_ = &structs[i];
        struct_->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->superStructLength, TZrSize);
        struct_->superStructs = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * struct_->superStructLength);
        io_read_references(io, struct_->superStructs, struct_->superStructLength);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->declaresLength, TZrSize);
        struct_->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * struct_->declaresLength);
        io_read_member_declares(io, struct_->declares, struct_->declaresLength);
    }
}

static void io_read_interfaces(SZrIo *io, SZrIoInterface *interfaces, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoInterface *interface = &interfaces[i];
        interface->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, interface->superInterfaceLength, TZrSize);
        interface->superInterfaces =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * interface->superInterfaceLength);
        io_read_references(io, interface->superInterfaces, interface->superInterfaceLength);
        ZR_IO_READ_NATIVE_TYPE(io, interface->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, interface->declaresLength, TZrSize);
        interface->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * interface->declaresLength);
        io_read_member_declares(io, interface->declares, interface->declaresLength);
    }
}

static void io_read_enums(SZrIo *io, SZrIoEnum *enums, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoEnum *enum_ = &enums[i];
        enum_->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->valueType, EZrValueType);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->fieldsLength, TZrSize);
        enum_->fields = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField) * enum_->fieldsLength);
        io_read_enum_fields(io, enum_->fields, enum_->fieldsLength, enum_->valueType);
    }
}
static void io_read_module_declares(SZrIo *io, SZrIoModuleDeclare *declares, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModuleDeclare *declare = &declares[i];
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        // check type and read
        switch (declare->type) {
            case ZR_IO_MODULE_DECLARE_TYPE_CLASS: {
                declare->class_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoClass));
                io_read_classes(io, declare->class_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION: {
                declare->function = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
                io_read_functions(io, declare->function, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_STRUCT: {
                declare->struct_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoStruct));
                io_read_structs(io, declare->struct_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_INTERFACE: {
                declare->interface_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoInterface));
                io_read_interfaces(io, declare->interface_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_ENUM: {
                declare->enum_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoEnum));
                io_read_enums(io, declare->enum_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FIELD: {
                declare->field = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField));
                io_read_field(io, declare->field);
            } break;
            default:
                break;
        }
    }
}

static void io_read_modules(SZrIo *io, SZrIoModule *modules, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModule *module = &modules[i];
        module->name = io_read_string_with_length(io);
        module->md5 = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, module->importsLength, TZrSize);
        module->imports = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoImport) * module->importsLength);
        io_read_imports(io, module->imports, module->importsLength);
        ZR_IO_READ_NATIVE_TYPE(io, module->declaresLength, TZrSize);
        module->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModuleDeclare) * module->declaresLength);
        io_read_module_declares(io, module->declares, module->declaresLength);
        module->entryFunction = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
        io_read_functions(io, module->entryFunction, 1);
    }
}

SZrIo *ZrCore_Io_New(struct SZrGlobalState *global) {
    SZrIo *io = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIo));
    io->state = ZR_NULL;
    io->read = ZR_NULL;
    io->close = ZR_NULL;
    io->customData = ZR_NULL;
    io->pointer = ZR_NULL;
    io->remained = 0;
    io->isBinary = ZR_FALSE;
    io->sourceVersionPatch = 0;
    return io;
}

void ZrCore_Io_Free(struct SZrGlobalState *global, SZrIo *io) {
    if (io != ZR_NULL) {
        ZR_IO_FREE_NATIVE_DATA(global, io, sizeof(SZrIo));
    }
}

void ZrCore_Io_Init(SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData) {
    io->state = state;
    io->read = read;
    io->close = close;
    io->customData = customData;
    io->pointer = ZR_NULL;
    io->remained = 0;
    io->isBinary = ZR_FALSE;
    io->sourceVersionPatch = 0;
}

TZrSize ZrCore_Io_Read(SZrIo *io, TZrBytePtr buffer, TZrSize size) {
    TZrSize requestedSize = size;

    while (size > 0) {
        if (io->remained == 0) {
            if (!io_refill(io)) {
                if (io->state != ZR_NULL) {
                    ZrCore_Debug_RunError(io->state, "io read refill failed");
                }
                return requestedSize - size;
            }
        }
        // todo: different endianness
        TZrSize read = (size <= io->remained) ? size : io->remained;
        ZrCore_Memory_RawCopy(buffer, io->pointer, read);
        io->remained -= read;
        io->pointer += read;
        buffer += read;
        size -= read;
    }

    return requestedSize;
}


SZrIoSource *ZrCore_Io_ReadSourceNew(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    SZrIoSource *source = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoSource));
    ZrCore_Io_Read(io, (TZrBytePtr) source->signature, sizeof(source->signature));
    // todo: check signature
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMajor, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMinor, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionPatch, TZrUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->format, TZrUInt64);
    io->sourceVersionPatch = source->versionPatch;
    ZR_IO_READ_NATIVE_TYPE(io, source->nativeIntSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeSizeSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeInstructionSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->isBigEndian, TZrBool);
    ZR_IO_READ_NATIVE_TYPE(io, source->isDebug, TZrBool);
    ZrCore_Io_Read(io, (TZrBytePtr) source->optional, sizeof(source->optional));
    if (source->versionPatch < ZR_IO_SOURCE_PATCH_HAS_MODULE_INIT_METADATA) {
        ZrCore_Debug_RunError(io->state, "io source version is too old for this runtime");
        return ZR_NULL;
    }
    ZR_IO_READ_NATIVE_TYPE(io, source->modulesLength, TZrSize);
    source->modules = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModule) * source->modulesLength);
    io_read_modules(io, source->modules, source->modulesLength);
    return source;
}

void ZrCore_Io_ReadSourceFree(struct SZrGlobalState *global, SZrIoSource *source) {
    ZR_UNUSED_PARAMETER(global);
    ZR_UNUSED_PARAMETER(source);
    // todo: after convert it to vm object, release it.
}

SZrIoSource *ZrCore_Io_LoadSource(struct SZrState *state, TZrNativeString sourceName, TZrNativeString md5) {
    SZrGlobalState *global = state->global;
    SZrIo *io = ZrCore_Io_New(global);
    TZrBool success = global->sourceLoader(state, sourceName, md5, io);
    if (!success) {
        ZrCore_Io_Free(global, io);
        return ZR_NULL;
    }
    SZrIoSource *source = ZrCore_Io_ReadSourceNew(io);
    if (io->close != ZR_NULL) {
        io->close(state, io->customData);
    }
    ZrCore_Io_Free(global, io);
    return source;
}
