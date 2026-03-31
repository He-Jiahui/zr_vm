//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
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

static void io_read_value(SZrIo *io, EZrValueType type, TZrPureValue *value) {
    ZR_UNUSED_PARAMETER(io);
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
            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(io_read_string_with_length(io));
        } break;
        default: {
            // todo:
        } break;
    }
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
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionStartIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionEndIndex, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TZrUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TZrUInt64);
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
        variable->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, variable->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, variable->accessModifier, TZrUInt8);
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

        symbol->name = io_read_string_with_length(io);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->stackSlot, TZrUInt32);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->accessModifier, TZrUInt8);
        ZR_IO_READ_NATIVE_TYPE(io, symbol->symbolKind, TZrUInt8);
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
        ZR_IO_READ_NATIVE_TYPE(io, debugInfo->instructionsLength, TZrSize);
        debugInfo->instructionsLine = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TZrUInt64) * debugInfo->instructionsLength);
        ZrCore_Io_Read(io, (TZrBytePtr) debugInfo->instructionsLine, sizeof(TZrUInt64) * debugInfo->instructionsLength);
        // todo:
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
        // 读取PROTOTYPES_LENGTH和PROTOTYPES（结构化格式）
        ZR_IO_READ_NATIVE_TYPE(io, function->prototypesLength, TZrSize);
        function->classes = ZR_NULL;
        function->structs = ZR_NULL;

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
                declare->class = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoClass));
                io_read_classes(io, declare->class, 1);
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
                declare->interface = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoInterface));
                io_read_interfaces(io, declare->interface, 1);
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
}

TZrSize ZrCore_Io_Read(SZrIo *io, TZrBytePtr buffer, TZrSize size) {
    TZrSize requestedSize = size;

    while (size > 0) {
        if (io->remained == 0) {
            if (!io_refill(io)) {
                if (io->state != ZR_NULL) {
                    ZrCore_Exception_Throw(io->state, ZR_THREAD_STATUS_RUNTIME_ERROR);
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
    ZR_IO_READ_NATIVE_TYPE(io, source->nativeIntSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeSizeSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeInstructionSize, TZrUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->isBigEndian, TZrBool);
    ZR_IO_READ_NATIVE_TYPE(io, source->isDebug, TZrBool);
    ZrCore_Io_Read(io, (TZrBytePtr) source->optional, sizeof(source->optional));
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
