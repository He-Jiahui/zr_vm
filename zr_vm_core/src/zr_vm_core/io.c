//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

#define ZR_IO_MALLOC_NATIVE_DATA(GLOBAL, SIZE) ZrMemoryRawMallocWithType(GLOBAL, (SIZE), ZR_VALUE_TYPE_NATIVE_DATA);
#define ZR_IO_FREE_NATIVE_DATA(GLOBAL, DATA, SIZE)                                                                     \
    ZrMemoryRawFreeWithType(GLOBAL, (DATA), (SIZE), ZR_VALUE_TYPE_NATIVE_DATA);
static TBool ZrIoRefill(SZrIo *io) {
    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }
    io->remained = readSize;
    io->pointer = buffer;
    return ZR_TRUE;
}

static TInt32 ZrIoReadChar(SZrIo *io) {
    if (io->remained == 0) {
        if (!ZrIoRefill(io)) {
            // EOF = -1
            return ZR_IO_EOF;
        }
    }
    // BYTE > 0 && BYTE < 256
    TInt32 c = *io->pointer;
    io->remained--;
    io->pointer++;
    return c;
}

static ZR_FORCE_INLINE TZrSize ZrIoReadSize(SZrIo *io) {
    TZrSize size;
    ZrIoRead(io, (TBytePtr) &size, sizeof(size));
    return size;
}

static ZR_FORCE_INLINE TFloat64 ZrIoReadFloat(SZrIo *io) {
    TFloat64 value;
    ZrIoRead(io, (TBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TInt64 ZrIoReadInt(SZrIo *io) {
    TInt64 value;
    ZrIoRead(io, (TBytePtr) &value, sizeof(value));
    return value;
}

static ZR_FORCE_INLINE TUInt64 ZrIoReadUInt(SZrIo *io) {
    TUInt64 value;
    ZrIoRead(io, (TBytePtr) &value, sizeof(value));
    return value;
}

#define ZR_IO_READ_RAW(IO, VALUE, SIZE) ZrIoRead(IO, &(VALUE), SIZE);

#define ZR_IO_READ_NATIVE_TYPE(IO, DATA, TYPE) ZrIoRead(IO, (TBytePtr) & (DATA), sizeof(TYPE))


static SZrString *ZrIoReadStringWithLength(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    TZrSize length = ZrIoReadSize(io);
    if (length == 0) {
        return ZR_NULL;
    }
    TNativeString nativeString = ZrMemoryRawMallocWithType(global, length + 1, ZR_VALUE_TYPE_STRING);
    ZrIoRead(io, (TBytePtr) nativeString, length);
    nativeString[length] = '\0';
    SZrString *string = ZrStringCreate(io->state, nativeString, length);
    ZrMemoryRawFree(global, nativeString, length + 1);
    return string;
}

static void ZrIoReadImports(SZrIo *io, SZrIoImport *imports, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoImport *import = &imports[i];
        import->name = ZrIoReadStringWithLength(io);
        import->md5 = ZrIoReadStringWithLength(io);
    }
}

static void ZrIoReadValue(SZrIo *io, EZrValueType type, TZrPureValue *value) {
    ZR_UNUSED_PARAMETER(io);
    switch (type) {
        case ZR_VALUE_TYPE_NULL: {
            value->nativeObject.nativeBool = ZR_FALSE;
        } break;
        case ZR_VALUE_TYPE_BOOL: {
            value->nativeObject.nativeBool = ZrIoReadChar(io);
        } break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE: {
            value->nativeObject.nativeDouble = ZrIoReadFloat(io);
        } break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: {
            value->nativeObject.nativeInt64 = ZrIoReadInt(io);
        } break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: {
            value->nativeObject.nativeUInt64 = ZrIoReadUInt(io);
        } break;
        case ZR_VALUE_TYPE_STRING: {
            value->object = ZR_CAST_RAW_OBJECT_AS_SUPER(ZrIoReadStringWithLength(io));
        } break;
        default: {
            // todo:
        } break;
    }
}

static void ZrIoReadField(SZrIo *io, SZrIoField *field) { field->name = ZrIoReadStringWithLength(io); }

static void ZrIoReadReferences(SZrIo *io, SZrIoReference *references, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoReference *reference = &references[i];
        reference->referenceModuleName = ZrIoReadStringWithLength(io);
        reference->referenceModuleMd5 = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, reference->referenceIndex, TZrSize);
    }
}

static void ZrIoReadFunctionLocalVariables(SZrIo *io, SZrIoFunctionLocalVariable *variables, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionLocalVariable *variable = &variables[i];
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionStartIndex, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->instructionEndIndex, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TUInt64);
    }
}

static void ZrIoReadFunctionConstantVariables(SZrIo *io, SZrIoFunctionConstantVariable *variables, TZrSize count) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionConstantVariable *variable = &variables[i];
        ZR_IO_READ_NATIVE_TYPE(io, variable->type, TUInt64);
        ZrIoReadValue(io, variable->type, &variable->value);
        ZR_IO_READ_NATIVE_TYPE(io, variable->startLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, variable->endLine, TUInt64);
    }
}

static void ZrIoReadFunctions(SZrIo *io, SZrIoFunction *functions, TZrSize count);

static void ZrIoReadFunctionClosures(SZrIo *io, SZrIoFunctionClosure *closures, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionClosure *closure = &closures[i];
        closure->subFunction = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
        ZrIoReadFunctions(io, closure->subFunction, 1);
        // todo:
    }
}

static void ZrIoReadFunctionDebugInfos(SZrIo *io, SZrIoFunctionDebugInfo *debugInfos, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunctionDebugInfo *debugInfo = &debugInfos[i];
        ZR_IO_READ_NATIVE_TYPE(io, debugInfo->instructionsLength, TZrSize);
        debugInfo->instructionsLine = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TUInt64) * debugInfo->instructionsLength);
        ZrIoRead(io, (TBytePtr) debugInfo->instructionsLine, sizeof(TUInt64) * debugInfo->instructionsLength);
        // todo:
    }
}


static void ZrIoReadFunctions(SZrIo *io, SZrIoFunction *functions, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunction *function = &functions[i];
        function->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, function->startLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->endLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->parametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, function->hasVarArgs, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->instructionsLength, TZrSize);
        // read instructions ...
        function->instructions =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(TZrInstruction) * function->instructionsLength);
        ZrIoRead(io, (TBytePtr) function->instructions, sizeof(TZrInstruction) * function->instructionsLength);
        ZR_IO_READ_NATIVE_TYPE(io, function->localVariablesLength, TZrSize);
        function->localVariables =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionLocalVariable) * function->localVariablesLength);
        ZrIoReadFunctionLocalVariables(io, function->localVariables, function->localVariablesLength);
        ZR_IO_READ_NATIVE_TYPE(io, function->constantVariablesLength, TZrSize);
        function->constantVariables = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionConstantVariable) *
                                                                               function->constantVariablesLength);
        ZrIoReadFunctionConstantVariables(io, function->constantVariables, function->constantVariablesLength);
        ZR_IO_READ_NATIVE_TYPE(io, function->closuresLength, TZrSize);
        function->closures = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction) * function->closuresLength);
        ZrIoReadFunctionClosures(io, function->closures, function->closuresLength);
        ZR_IO_READ_NATIVE_TYPE(io, function->debugInfosLength, TZrSize);
        function->debugInfos =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunctionDebugInfo) * function->debugInfosLength);
        ZrIoReadFunctionDebugInfos(io, function->debugInfos, function->debugInfosLength);
    }
}

static void ZrIoReadMethod(SZrIo *io, SZrIoMethod *method) {
    SZrGlobalState *global = io->state->global;
    method->name = ZrIoReadStringWithLength(io);
    ZR_IO_READ_NATIVE_TYPE(io, method->functionsLength, TZrSize);
    method->functions = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction) * method->functionsLength);
    ZrIoReadFunctions(io, method->functions, method->functionsLength);
}


static void ZrIoReadProperty(SZrIo *io, SZrIoProperty *property) {
    SZrGlobalState *global = io->state->global;
    property->name = ZrIoReadStringWithLength(io);
    // todo: use enum
    ZR_IO_READ_NATIVE_TYPE(io, property->propertyType, TUInt32);
    property->getter = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
    ZrIoReadFunctions(io, property->getter, 1);
    property->setter = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
    ZrIoReadFunctions(io, property->setter, 1);
}

static void ZrIoReadMeta(SZrIo *io, SZrIoMeta *meta) {
    SZrGlobalState *global = io->state->global;
    // todo: use enum
    ZR_IO_READ_NATIVE_TYPE(io, meta->metaType, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, meta->functionsLength, TZrSize);
    meta->functions = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction) * meta->functionsLength);
    ZrIoReadFunctions(io, meta->functions, meta->functionsLength);
}

static void ZrIoReadEnumFields(SZrIo *io, SZrIoEnumField *fields, TZrSize count, EZrValueType valueType) {
    // SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoEnumField *field = &fields[i];
        field->name = ZrIoReadStringWithLength(io);
        ZrIoReadValue(io, valueType, &field->value);
    }
}

static void ZrIoReadMemberDeclares(SZrIo *io, SZrIoMemberDeclare *declares, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoMemberDeclare *declare = &declares[i];
        // todo:
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        switch (declare->type) {
            case ZR_IO_MEMBER_DECLARE_TYPE_METHOD: {
                // ZrIoReadFunctions(io, declare->function, 1);
                declare->method = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMethod));
                ZrIoReadMethod(io, declare->method);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_META: {
                declare->meta = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMeta));
                ZrIoReadMeta(io, declare->meta);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY: {
                declare->property = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoProperty));
                ZrIoReadProperty(io, declare->property);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_ENUM: {
                declare->enumField = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoEnumField));
                ZrIoReadEnumFields(io, declare->enumField, 1, ZR_VALUE_TYPE_UINT64);
            } break;
            case ZR_IO_MEMBER_DECLARE_TYPE_FIELD: {
                declare->field = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField));
                ZrIoReadField(io, declare->field);
            } break;
            default:
                break;
        }
        // check type and read
    }
}

static void ZrIoReadClasses(SZrIo *io, SZrIoClass *classes, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoClass *class = &classes[i];
        class->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, class->superClassLength, TZrSize);
        class->superClasses = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * class->superClassLength);
        ZrIoReadReferences(io, class->superClasses, class->superClassLength);
        ZR_IO_READ_NATIVE_TYPE(io, class->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, class->declaresLength, TZrSize);
        class->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * class->declaresLength);
        ZrIoReadMemberDeclares(io, class->declares, class->declaresLength);
    }
}

static void ZrIoReadStructs(SZrIo *io, SZrIoStruct *structs, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoStruct *struct_ = &structs[i];
        struct_->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->superStructLength, TZrSize);
        struct_->superStructs = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * struct_->superStructLength);
        ZrIoReadReferences(io, struct_->superStructs, struct_->superStructLength);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, struct_->declaresLength, TZrSize);
        struct_->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * struct_->declaresLength);
        ZrIoReadMemberDeclares(io, struct_->declares, struct_->declaresLength);
    }
}

static void ZrIoReadInterfaces(SZrIo *io, SZrIoInterface *interfaces, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoInterface *interface = &interfaces[i];
        interface->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, interface->superInterfaceLength, TZrSize);
        interface->superInterfaces =
                ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoReference) * interface->superInterfaceLength);
        ZrIoReadReferences(io, interface->superInterfaces, interface->superInterfaceLength);
        ZR_IO_READ_NATIVE_TYPE(io, interface->genericParametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, interface->declaresLength, TZrSize);
        interface->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoMemberDeclare) * interface->declaresLength);
        ZrIoReadMemberDeclares(io, interface->declares, interface->declaresLength);
    }
}

static void ZrIoReadEnums(SZrIo *io, SZrIoEnum *enums, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoEnum *enum_ = &enums[i];
        enum_->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->valueType, EZrValueType);
        ZR_IO_READ_NATIVE_TYPE(io, enum_->fieldsLength, TZrSize);
        enum_->fields = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField) * enum_->fieldsLength);
        ZrIoReadEnumFields(io, enum_->fields, enum_->fieldsLength, enum_->valueType);
    }
}
static void ZrIoReadModuleDeclares(SZrIo *io, SZrIoModuleDeclare *declares, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModuleDeclare *declare = &declares[i];
        ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoMemberDeclareType);
        // check type and read
        switch (declare->type) {
            case ZR_IO_MODULE_DECLARE_TYPE_CLASS: {
                declare->class = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoClass));
                ZrIoReadClasses(io, declare->class, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION: {
                declare->function = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
                ZrIoReadFunctions(io, declare->function, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_STRUCT: {
                declare->struct_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoStruct));
                ZrIoReadStructs(io, declare->struct_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_INTERFACE: {
                declare->interface = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoInterface));
                ZrIoReadInterfaces(io, declare->interface, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_ENUM: {
                declare->enum_ = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoEnum));
                ZrIoReadEnums(io, declare->enum_, 1);
            } break;
            case ZR_IO_MODULE_DECLARE_TYPE_FIELD: {
                declare->field = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoField));
                ZrIoReadField(io, declare->field);
            } break;
            default:
                break;
        }
    }
}

static void ZrIoReadModules(SZrIo *io, SZrIoModule *modules, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModule *module = &modules[i];
        module->name = ZrIoReadStringWithLength(io);
        module->md5 = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, module->importsLength, TZrSize);
        module->imports = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoImport) * module->importsLength);
        ZrIoReadImports(io, module->imports, module->importsLength);
        ZR_IO_READ_NATIVE_TYPE(io, module->declaresLength, TZrSize);
        module->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModuleDeclare) * module->declaresLength);
        ZrIoReadModuleDeclares(io, module->declares, module->declaresLength);
        module->entryFunction = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoFunction));
        ZrIoReadFunctions(io, module->entryFunction, 1);
    }
}

SZrIo *ZrIoNew(struct SZrGlobalState *global) {
    SZrIo *io = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIo));
    io->state = ZR_NULL;
    io->read = ZR_NULL;
    io->close = ZR_NULL;
    io->customData = ZR_NULL;
    io->pointer = ZR_NULL;
    io->remained = 0;
    return io;
}

void ZrIoFree(struct SZrGlobalState *global, SZrIo *io) {
    if (io != ZR_NULL) {
        ZR_IO_FREE_NATIVE_DATA(global, io, sizeof(SZrIo));
    }
}

void ZrIoInit(SZrState *state, SZrIo *io, FZrIoRead read, FZrIoClose close, TZrPtr customData) {
    io->state = state;
    io->read = read;
    io->close = close;
    io->customData = customData;
    io->pointer = ZR_NULL;
    io->remained = 0;
}

TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size) {
    while (size > 0) {
        if (io->remained == 0) {
            if (!ZrIoRefill(io)) {
                return size;
            }
        }
        // todo: different endianness
        TZrSize read = (size <= io->remained) ? size : io->remained;
        ZrMemoryRawCopy(buffer, io->pointer, read);
        io->remained -= read;
        io->pointer += read;
        buffer += read;
        size -= read;
    }
    // throw error
    // TODO:
    // ZrExceptionThrow(io->state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    return 0;
}


SZrIoSource *ZrIoReadSourceNew(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    SZrIoSource *source = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoSource));
    ZrIoRead(io, (TBytePtr) source->signature, sizeof(source->signature));
    // todo: check signature
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMajor, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMinor, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionPatch, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->format, TUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, source->nativeIntSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeSizeSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeInstructionSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->isBigEndian, TBool);
    ZR_IO_READ_NATIVE_TYPE(io, source->isDebug, TBool);
    ZrIoRead(io, (TBytePtr) source->optional, sizeof(source->optional));
    ZR_IO_READ_NATIVE_TYPE(io, source->modulesLength, TZrSize);
    source->modules = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModule) * source->modulesLength);
    ZrIoReadModules(io, source->modules, source->modulesLength);
    return source;
}

void ZrIoReadSourceFree(struct SZrGlobalState *global, SZrIoSource *source) {
    // todo: after convert it to vm object, release it.
}

SZrIoSource *ZrIoLoadSource(struct SZrState *state, TNativeString sourceName, TNativeString md5) {
    SZrGlobalState *global = state->global;
    SZrIo *io = ZrIoNew(global);
    TBool success = global->sourceLoader(state, sourceName, md5, io);
    if (!success) {
        ZrIoFree(global, io);
        return ZR_NULL;
    }
    SZrIoSource *source = ZrIoReadSourceNew(io);
    if (io->close != ZR_NULL) {
        io->close(state, io->customData);
    }
    ZrIoFree(global, io);
    return source;
}
