//
// Compile-time import metadata loading for native/source/binary modules.
//

#include "compiler_internal.h"
#include "module_init_analysis.h"
#include "type_inference_internal.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/io.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static TZrBool import_metadata_trace_enabled(void);
static void import_metadata_trace(const TZrChar *format, ...);

static void io_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                          const SZrIoFunctionTypedTypeRef *typeRef,
                                          SZrInferredType *result);

static TZrBool import_io_constant_to_value(SZrState *state,
                                           const SZrIoFunctionConstantVariable *source,
                                           SZrTypeValue *result) {
    if (state == ZR_NULL || source == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    switch (source->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_TRUE;

        case ZR_VALUE_TYPE_BOOL:
            result->type = ZR_VALUE_TYPE_BOOL;
            result->value.nativeObject.nativeBool = source->value.nativeObject.nativeBool;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            result->type = source->type;
            result->value.nativeObject.nativeInt64 = source->value.nativeObject.nativeInt64;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            result->type = source->type;
            result->value.nativeObject.nativeUInt64 = source->value.nativeObject.nativeUInt64;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            result->type = source->type;
            result->value.nativeObject.nativeDouble = source->value.nativeObject.nativeDouble;
            result->isGarbageCollectable = ZR_FALSE;
            result->isNative = ZR_TRUE;
            return ZR_TRUE;

        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            if (source->value.object == ZR_NULL) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsRawObject(state, result, source->value.object);
            result->type = source->type;
            return ZR_TRUE;

        default:
            return ZR_FALSE;
    }
}

static TZrBool refill_import_io(SZrIo *io) {
    SZrState *state;
    TZrSize readSize = 0;
    TZrBytePtr buffer;

    if (io == ZR_NULL || io->state == ZR_NULL || io->read == ZR_NULL) {
        return ZR_FALSE;
    }

    state = io->state;
    ZR_THREAD_UNLOCK(state);
    buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr read_all_import_bytes(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize capacity;
    TZrSize totalSize = 0;
    TZrBytePtr buffer;

    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL || state->global == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = io->remained > 0 ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    while (io->remained > 0 || refill_import_io(io)) {
        TZrSize chunkSize = io->remained;
        if (totalSize + chunkSize + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr resizedBuffer;

            while (totalSize + chunkSize + 1 > newCapacity) {
                newCapacity *= 2;
            }

            resizedBuffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global,
                                                                        newCapacity + 1,
                                                                        ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (resizedBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(resizedBuffer, buffer, totalSize);
            }
            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = resizedBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, chunkSize);
        totalSize += chunkSize;
        io->pointer += chunkSize;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

static void import_type_prototype_init(SZrState *state,
                                       SZrTypePrototypeInfo *info,
                                       SZrString *name,
                                       EZrObjectPrototypeType type) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(info, 0, sizeof(*info));
    info->name = name;
    info->type = type;
    info->accessModifier = ZR_ACCESS_PUBLIC;
    info->isImportedNative = ZR_TRUE;
    info->protocolMask = 0;
    info->allowValueConstruction = type != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE &&
                                   type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction = info->allowValueConstruction;
    info->hasDecoratorMetadata = ZR_FALSE;
    ZrCore_Value_ResetAsNull(&info->decoratorMetadataValue);
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
}

static SZrString *import_metadata_default_builtin_root_name(SZrState *state, EZrObjectPrototypeType type) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    if (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return ZrCore_String_Create(state, "zr.builtin.Module", strlen("zr.builtin.Module"));
    }
    if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        return ZrCore_String_Create(state, "zr.builtin.Object", strlen("zr.builtin.Object"));
    }

    return ZR_NULL;
}

static void import_metadata_apply_default_builtin_root(SZrState *state,
                                                       SZrTypePrototypeInfo *info,
                                                       EZrObjectPrototypeType type) {
    SZrString *defaultRootName;

    if (state == ZR_NULL || info == ZR_NULL || info->extendsTypeName != ZR_NULL) {
        return;
    }

    defaultRootName = import_metadata_default_builtin_root_name(state, type);
    if (defaultRootName == ZR_NULL) {
        return;
    }

    info->extendsTypeName = defaultRootName;
    ZrCore_Array_Push(state, &info->inherits, &defaultRootName);
}

static TZrBool import_prototype_has_member(SZrTypePrototypeInfo *info, SZrString *memberName) {
    if (info == ZR_NULL || memberName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < info->members.length; index++) {
        SZrTypeMemberInfo *existing = (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, memberName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool import_compile_info_stack_contains(SZrGlobalState *global, SZrString *moduleName) {
    if (global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < global->importCompileInfoStack.length; index++) {
        SZrString **entryPtr = (SZrString **)ZrCore_Array_Get(&global->importCompileInfoStack, index);
        if (entryPtr != ZR_NULL && *entryPtr != ZR_NULL && ZrCore_String_Equal(*entryPtr, moduleName)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrNativeString import_metadata_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static void report_import_compile_info_failure(SZrCompilerState *cs, SZrString *moduleName) {
    const SZrParserModuleInitSummary *summary;
    SZrFileRange location;
    TZrChar detail[ZR_PARSER_DETAIL_BUFFER_LENGTH];
    TZrNativeString moduleNameText;

    if (cs == ZR_NULL || moduleName == ZR_NULL || cs->hasError) {
        return;
    }

    ZrCore_Memory_RawSet(&location, 0, sizeof(location));
    summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, moduleName);
    if (summary != ZR_NULL && summary->state == ZR_PARSER_MODULE_INIT_SUMMARY_FAILED &&
        summary->errorMessage[0] != '\0') {
        ZrParser_Compiler_Error(cs, summary->errorMessage, summary->errorLocation);
        return;
    }

    if (cs->currentAst != ZR_NULL) {
        location = cs->currentAst->location;
    }
    moduleNameText = import_metadata_string_text(moduleName);
    snprintf(detail,
             sizeof(detail),
             "failed to load import metadata for module '%s'",
             moduleNameText != ZR_NULL ? moduleNameText : "<module>");
    ZrParser_Compiler_Error(cs, detail, location);
}

static SZrTypePrototypeInfo *find_registered_type_prototype_inference_exact_only(SZrCompilerState *cs,
                                                                                  SZrString *typeName) {
    if (cs == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && ZrCore_String_Equal(info->name, typeName)) {
            return info;
        }
    }

    if (cs->currentTypePrototypeInfo != ZR_NULL &&
        cs->typePrototypes.isValid &&
        cs->typePrototypes.head != ZR_NULL &&
        cs->typePrototypes.elementSize == sizeof(SZrTypePrototypeInfo) &&
        (const TZrUInt8 *)cs->currentTypePrototypeInfo >= (const TZrUInt8 *)cs->typePrototypes.head &&
        (const TZrUInt8 *)cs->currentTypePrototypeInfo <
                (const TZrUInt8 *)cs->typePrototypes.head +
                        cs->typePrototypes.elementSize * cs->typePrototypes.length &&
        ((((const TZrUInt8 *)cs->currentTypePrototypeInfo - (const TZrUInt8 *)cs->typePrototypes.head) %
          cs->typePrototypes.elementSize) == 0) &&
        cs->currentTypePrototypeInfo->name != ZR_NULL &&
        ZrCore_String_Equal(cs->currentTypePrototypeInfo->name, typeName)) {
        return cs->currentTypePrototypeInfo;
    }

    return ZR_NULL;
}

static void register_imported_type_name(SZrCompilerState *cs, SZrString *typeName) {
    (void)cs;
    (void)typeName;
}

static void function_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                                const SZrFunctionTypedTypeRef *typeRef,
                                                SZrInferredType *result) {
    if (cs == ZR_NULL || result == ZR_NULL) {
        return;
    }

    if (typeRef == ZR_NULL) {
        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_OBJECT);
        return;
    }

    if (typeRef->isArray) {
        SZrInferredType elementType;

        ZrParser_InferredType_Init(cs->state, result, ZR_VALUE_TYPE_ARRAY);
        result->ownershipQualifier = typeRef->ownershipQualifier;
        result->isNullable = typeRef->isNullable;
        ZrCore_Array_Init(cs->state, &result->elementTypes, sizeof(SZrInferredType), 1);
        ZrParser_InferredType_InitFull(cs->state,
                                       &elementType,
                                       typeRef->elementBaseType,
                                       ZR_FALSE,
                                       typeRef->elementTypeName);
        ZrCore_Array_Push(cs->state, &result->elementTypes, &elementType);
        return;
    }

    if (typeRef->typeName != ZR_NULL) {
        ZrParser_InferredType_InitFull(cs->state,
                                       result,
                                       typeRef->baseType,
                                       typeRef->isNullable,
                                       typeRef->typeName);
    } else {
        ZrParser_InferredType_Init(cs->state, result, typeRef->baseType);
        result->isNullable = typeRef->isNullable;
    }
    result->ownershipQualifier = typeRef->ownershipQualifier;
}

static SZrString *primitive_type_name(SZrCompilerState *cs, EZrValueType baseType) {
    const TZrChar *name = "object";

    if (cs == ZR_NULL) {
        return ZR_NULL;
    }

    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            name = "null";
            break;
        case ZR_VALUE_TYPE_BOOL:
            name = "bool";
            break;
        case ZR_VALUE_TYPE_INT8:
            name = "i8";
            break;
        case ZR_VALUE_TYPE_INT16:
            name = "i16";
            break;
        case ZR_VALUE_TYPE_INT32:
            name = "i32";
            break;
        case ZR_VALUE_TYPE_INT64:
            name = "int";
            break;
        case ZR_VALUE_TYPE_UINT8:
            name = "u8";
            break;
        case ZR_VALUE_TYPE_UINT16:
            name = "u16";
            break;
        case ZR_VALUE_TYPE_UINT32:
            name = "u32";
            break;
        case ZR_VALUE_TYPE_UINT64:
            name = "uint";
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            name = "float";
            break;
        case ZR_VALUE_TYPE_STRING:
            name = "string";
            break;
        case ZR_VALUE_TYPE_CLOSURE:
        case ZR_VALUE_TYPE_FUNCTION:
            name = "function";
            break;
        case ZR_VALUE_TYPE_ARRAY:
            name = "array";
            break;
        default:
            break;
    }

    return ZrCore_String_CreateFromNative(cs->state, (TZrNativeString)name);
}

static const SZrFunction *find_runtime_function_metadata_recursive(const SZrFunction *function,
                                                                   SZrString *name,
                                                                   TZrUInt32 parameterCount) {
    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->functionName != ZR_NULL &&
        ZrCore_String_Equal(function->functionName, name) &&
        (function->parameterMetadataCount == parameterCount || function->parameterCount == parameterCount ||
         parameterCount == 0)) {
        return function;
    }

    for (TZrUInt32 index = 0; index < function->childFunctionLength; index++) {
        const SZrFunction *match =
                find_runtime_function_metadata_recursive(&function->childFunctionList[index], name, parameterCount);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static const SZrIoFunction *find_io_function_metadata_recursive(const SZrIoFunction *function,
                                                                SZrString *name,
                                                                TZrSize parameterCount) {
    if (function == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    if (function->name != ZR_NULL &&
        ZrCore_String_Equal(function->name, name) &&
        (function->parameterMetadataLength == parameterCount || function->parametersLength == parameterCount ||
         parameterCount == 0)) {
        return function;
    }

    for (TZrSize index = 0; index < function->closuresLength; index++) {
        const SZrIoFunction *match =
                find_io_function_metadata_recursive(function->closures[index].subFunction, name, parameterCount);
        if (match != ZR_NULL) {
            return match;
        }
    }

    return ZR_NULL;
}

static void import_copy_runtime_parameter_metadata(SZrCompilerState *cs,
                                                   SZrTypeMemberInfo *memberInfo,
                                                   const SZrFunction *function) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || function == ZR_NULL || function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataCount == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterTypes,
                      sizeof(SZrInferredType),
                      function->parameterMetadataCount);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterNames,
                      sizeof(SZrString *),
                      function->parameterMetadataCount);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterHasDefaultValues,
                      sizeof(TZrBool),
                      function->parameterMetadataCount);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterDefaultValues,
                      sizeof(SZrTypeValue),
                      function->parameterMetadataCount);
    for (TZrUInt32 index = 0; index < function->parameterMetadataCount; index++) {
        SZrString *name = function->parameterMetadata[index].name;
        TZrBool hasDefaultValue = function->parameterMetadata[index].hasDefaultValue;
        SZrTypeValue defaultValue;
        SZrInferredType parameterType;
        ZrCore_Value_ResetAsNull(&defaultValue);
        ZrParser_InferredType_Init(cs->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
        if (hasDefaultValue) {
            defaultValue = function->parameterMetadata[index].defaultValue;
        }
        function_typed_type_ref_to_inferred(cs, &function->parameterMetadata[index].type, &parameterType);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterTypes, &parameterType);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterNames, &name);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterHasDefaultValues, &hasDefaultValue);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterDefaultValues, &defaultValue);
    }
}

static void import_copy_io_parameter_metadata(SZrCompilerState *cs,
                                              SZrTypeMemberInfo *memberInfo,
                                              const SZrIoFunction *function) {
    if (cs == ZR_NULL || memberInfo == ZR_NULL || function == ZR_NULL || function->parameterMetadata == ZR_NULL ||
        function->parameterMetadataLength == 0) {
        return;
    }

    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterTypes,
                      sizeof(SZrInferredType),
                      function->parameterMetadataLength);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterNames,
                      sizeof(SZrString *),
                      function->parameterMetadataLength);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterHasDefaultValues,
                      sizeof(TZrBool),
                      function->parameterMetadataLength);
    ZrCore_Array_Init(cs->state,
                      &memberInfo->parameterDefaultValues,
                      sizeof(SZrTypeValue),
                      function->parameterMetadataLength);
    for (TZrSize index = 0; index < function->parameterMetadataLength; index++) {
        SZrString *name = function->parameterMetadata[index].name;
        TZrBool hasDefaultValue = function->parameterMetadata[index].hasDefaultValue ? ZR_TRUE : ZR_FALSE;
        SZrTypeValue defaultValue;
        SZrInferredType parameterType;
        ZrCore_Value_ResetAsNull(&defaultValue);
        ZrParser_InferredType_Init(cs->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
        if (hasDefaultValue &&
            !import_io_constant_to_value(cs->state, &function->parameterMetadata[index].defaultValue, &defaultValue)) {
            ZrParser_InferredType_Free(cs->state, &parameterType);
            return;
        }
        io_typed_type_ref_to_inferred(cs, &function->parameterMetadata[index].type, &parameterType);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterTypes, &parameterType);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterNames, &name);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterHasDefaultValues, &hasDefaultValue);
        ZrCore_Array_Push(cs->state, &memberInfo->parameterDefaultValues, &defaultValue);
    }
}

static const SZrFunction *resolve_runtime_member_metadata_function(SZrState *state,
                                                                   const SZrFunction *ownerFunction,
                                                                   const SZrCompiledMemberInfo *compiledMember,
                                                                   const SZrTypeMemberInfo *memberInfo) {
    const SZrFunction *metadataFunction = ZR_NULL;

    if (state == ZR_NULL || ownerFunction == ZR_NULL || compiledMember == ZR_NULL) {
        return ZR_NULL;
    }

    if (compiledMember->functionConstantIndex < ownerFunction->constantValueLength) {
        metadataFunction = ZrCore_Closure_GetMetadataFunctionFromValue(state,
                                                                       &ownerFunction->constantValueList[compiledMember->functionConstantIndex]);
    }

    if (metadataFunction == ZR_NULL &&
        memberInfo != ZR_NULL &&
        memberInfo->name != ZR_NULL) {
        metadataFunction = find_runtime_function_metadata_recursive(ownerFunction,
                                                                    memberInfo->name,
                                                                    memberInfo->parameterCount);
    }

    return metadataFunction;
}

static SZrString *typed_type_ref_to_type_name(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef) {
    TZrNativeString elementName;
    TZrSize elementLength;
    TZrSize totalLength;
    TZrChar *buffer;
    SZrString *result;
    SZrString *elementType;

    if (cs == ZR_NULL || typeRef == ZR_NULL) {
        return ZR_NULL;
    }

    if (!typeRef->isArray) {
        return typeRef->typeName != ZR_NULL ? typeRef->typeName : primitive_type_name(cs, typeRef->baseType);
    }

    if (typeRef->elementTypeName != ZR_NULL) {
        elementType = typeRef->elementTypeName;
    } else {
        elementType = primitive_type_name(cs, typeRef->elementBaseType);
    }
    if (elementType == ZR_NULL) {
        return ZR_NULL;
    }

    elementName = ZrCore_String_GetNativeString(elementType);
    if (elementName == ZR_NULL) {
        return ZR_NULL;
    }

    elementLength = strlen(elementName);
    totalLength = elementLength + 2;
    buffer = (TZrChar *)ZrCore_Memory_RawMalloc(cs->state->global, totalLength + 1);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    memcpy(buffer, elementName, elementLength);
    buffer[elementLength] = '[';
    buffer[elementLength + 1] = ']';
    buffer[elementLength + 2] = '\0';
    result = ZrCore_String_Create(cs->state, buffer, totalLength);
    ZrCore_Memory_RawFree(cs->state->global, buffer, totalLength + 1);
    return result;
}

static void io_typed_type_ref_to_inferred(SZrCompilerState *cs,
                                          const SZrIoFunctionTypedTypeRef *typeRef,
                                          SZrInferredType *result) {
    SZrFunctionTypedTypeRef bridge;

    if (result == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(&bridge, 0, sizeof(bridge));
    if (typeRef != ZR_NULL) {
        bridge.baseType = typeRef->baseType;
        bridge.isNullable = typeRef->isNullable;
        bridge.ownershipQualifier = typeRef->ownershipQualifier;
        bridge.isArray = typeRef->isArray;
        bridge.typeName = typeRef->typeName;
        bridge.elementBaseType = typeRef->elementBaseType;
        bridge.elementTypeName = typeRef->elementTypeName;
    }
    function_typed_type_ref_to_inferred(cs, &bridge, result);
}

static SZrString *io_typed_type_ref_to_type_name(SZrCompilerState *cs, const SZrIoFunctionTypedTypeRef *typeRef) {
    SZrFunctionTypedTypeRef bridge;

    ZrCore_Memory_RawSet(&bridge, 0, sizeof(bridge));
    if (typeRef != ZR_NULL) {
        bridge.baseType = typeRef->baseType;
        bridge.isNullable = typeRef->isNullable;
        bridge.ownershipQualifier = typeRef->ownershipQualifier;
        bridge.isArray = typeRef->isArray;
        bridge.typeName = typeRef->typeName;
        bridge.elementBaseType = typeRef->elementBaseType;
        bridge.elementTypeName = typeRef->elementTypeName;
    }
    return typed_type_ref_to_type_name(cs, &bridge);
}

static void import_add_field_member(SZrState *state,
                                    SZrTypePrototypeInfo *info,
                                    SZrString *memberName,
                                    SZrString *fieldTypeName,
                                    TZrBool isStatic,
                                    EZrModuleExportKind exportKind,
                                    EZrModuleExportReadiness readiness) {
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || info == ZR_NULL || memberName == ZR_NULL || import_prototype_has_member(info, memberName)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    ZrCore_Array_Construct(&memberInfo.parameterTypes);
    ZrCore_Array_Construct(&memberInfo.parameterNames);
    ZrCore_Array_Construct(&memberInfo.parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo.parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo.genericParameters);
    ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo.decorators);
    ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
    memberInfo.memberType = info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT ? ZR_AST_STRUCT_FIELD : ZR_AST_CLASS_FIELD;
    memberInfo.name = memberName;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = isStatic;
    memberInfo.fieldTypeName = fieldTypeName;
    memberInfo.moduleExportKind = exportKind;
    memberInfo.moduleExportReadiness = readiness;
    ZrCore_Array_Push(state, &info->members, &memberInfo);
}

static void import_add_function_member_from_symbol(SZrCompilerState *cs,
                                                   SZrTypePrototypeInfo *modulePrototype,
                                                   const SZrFunctionTypedExportSymbol *symbol,
                                                   const SZrFunction *moduleFunction) {
    SZrTypeMemberInfo memberInfo;
    const SZrFunction *metadataFunction;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL ||
        import_prototype_has_member(modulePrototype, symbol->name)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    ZrCore_Array_Construct(&memberInfo.parameterTypes);
    ZrCore_Array_Construct(&memberInfo.parameterNames);
    ZrCore_Array_Construct(&memberInfo.parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo.parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo.genericParameters);
    ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo.decorators);
    ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
    memberInfo.memberType = ZR_AST_CLASS_METHOD;
    memberInfo.name = symbol->name;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = ZR_TRUE;
    memberInfo.parameterCount = symbol->parameterCount;
    memberInfo.returnTypeName = typed_type_ref_to_type_name(cs, &symbol->valueType);
    memberInfo.moduleExportKind = (EZrModuleExportKind)symbol->exportKind;
    memberInfo.moduleExportReadiness = (EZrModuleExportReadiness)symbol->readiness;
    if (symbol->parameterCount > 0 && symbol->parameterTypes != ZR_NULL) {
        ZrCore_Array_Init(cs->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), symbol->parameterCount);
        for (TZrUInt32 paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
            SZrInferredType paramType;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            function_typed_type_ref_to_inferred(cs, &symbol->parameterTypes[paramIndex], &paramType);
            ZrCore_Array_Push(cs->state, &memberInfo.parameterTypes, &paramType);
        }
    }

    metadataFunction =
            find_runtime_function_metadata_recursive(moduleFunction, symbol->name, symbol->parameterCount);
    import_copy_runtime_parameter_metadata(cs, &memberInfo, metadataFunction);

    ZrCore_Array_Push(cs->state, &modulePrototype->members, &memberInfo);
}

static void import_add_function_member_from_io_symbol(SZrCompilerState *cs,
                                                      SZrTypePrototypeInfo *modulePrototype,
                                                      const SZrIoFunctionTypedExportSymbol *symbol,
                                                      const SZrIoFunction *moduleFunction) {
    SZrTypeMemberInfo memberInfo;
    const SZrIoFunction *metadataFunction;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL ||
        import_prototype_has_member(modulePrototype, symbol->name)) {
        return;
    }

    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    ZrCore_Array_Construct(&memberInfo.parameterTypes);
    ZrCore_Array_Construct(&memberInfo.parameterNames);
    ZrCore_Array_Construct(&memberInfo.parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo.parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo.genericParameters);
    ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo.decorators);
    ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
    memberInfo.memberType = ZR_AST_CLASS_METHOD;
    memberInfo.name = symbol->name;
    memberInfo.accessModifier = ZR_ACCESS_PUBLIC;
    memberInfo.isStatic = ZR_TRUE;
    memberInfo.parameterCount = (TZrUInt32)symbol->parameterCount;
    memberInfo.returnTypeName = io_typed_type_ref_to_type_name(cs, &symbol->valueType);
    memberInfo.moduleExportKind = (EZrModuleExportKind)symbol->exportKind;
    memberInfo.moduleExportReadiness = (EZrModuleExportReadiness)symbol->readiness;
    if (symbol->parameterCount > 0 && symbol->parameterTypes != ZR_NULL) {
        ZrCore_Array_Init(cs->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), symbol->parameterCount);
        for (TZrSize paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
            SZrInferredType paramType;

            ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
            io_typed_type_ref_to_inferred(cs, &symbol->parameterTypes[paramIndex], &paramType);
            ZrCore_Array_Push(cs->state, &memberInfo.parameterTypes, &paramType);
        }
    }

    metadataFunction = find_io_function_metadata_recursive(moduleFunction, symbol->name, symbol->parameterCount);
    import_copy_io_parameter_metadata(cs, &memberInfo, metadataFunction);

    ZrCore_Array_Push(cs->state, &modulePrototype->members, &memberInfo);
}

static SZrString *function_constant_string(SZrState *state, const SZrFunction *function, TZrUInt32 index) {
    const SZrTypeValue *constant;

    if (state == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        index >= function->constantValueLength) {
        return ZR_NULL;
    }

    constant = &function->constantValueList[index];
    if (constant->type != ZR_VALUE_TYPE_STRING || constant->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_STRING(state, constant->value.object);
}

static TZrBool import_append_decorator_names_from_constant_array(SZrState *state,
                                                                 SZrArray *decorators,
                                                                 const SZrTypeValue *decoratorArrayValue) {
    SZrObject *decoratorArray;

    if (state == ZR_NULL || decorators == ZR_NULL || decoratorArrayValue == ZR_NULL ||
        decoratorArrayValue->type != ZR_VALUE_TYPE_ARRAY || decoratorArrayValue->value.object == ZR_NULL) {
        return ZR_FALSE;
    }

    decoratorArray = ZR_CAST_OBJECT(state, decoratorArrayValue->value.object);
    if (decoratorArray == ZR_NULL || decoratorArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }

    if (!decorators->isValid || decorators->elementSize == 0) {
        ZrCore_Array_Init(state,
                          decorators,
                          sizeof(SZrTypeDecoratorInfo),
                          decoratorArray->nodeMap.elementCount > 0 ? decoratorArray->nodeMap.elementCount
                                                                   : ZR_PARSER_INITIAL_CAPACITY_TINY);
    }

    for (TZrSize decoratorIndex = 0; decoratorIndex < decoratorArray->nodeMap.elementCount; decoratorIndex++) {
        SZrTypeValue key;
        const SZrTypeValue *decoratorNameValue;
        SZrTypeDecoratorInfo decoratorInfo;

        ZrCore_Value_InitAsInt(state, &key, (TZrInt64)decoratorIndex);
        decoratorNameValue = ZrCore_Object_GetValue(state, decoratorArray, &key);
        if (decoratorNameValue == ZR_NULL || decoratorNameValue->type != ZR_VALUE_TYPE_STRING ||
            decoratorNameValue->value.object == ZR_NULL) {
            continue;
        }

        ZrCore_Memory_RawSet(&decoratorInfo, 0, sizeof(decoratorInfo));
        decoratorInfo.name = ZR_CAST_STRING(state, decoratorNameValue->value.object);
        ZrCore_Array_Push(state, decorators, &decoratorInfo);
    }

    return ZR_TRUE;
}

static TZrBool import_append_member_decorator_names_from_function_constant(SZrState *state,
                                                                           SZrArray *decorators,
                                                                           const SZrFunction *function,
                                                                           TZrUInt32 constantIndex) {
    const SZrTypeValue *constantValue;

    if (state == ZR_NULL || decorators == ZR_NULL || function == ZR_NULL || function->constantValueList == ZR_NULL ||
        constantIndex >= function->constantValueLength) {
        return ZR_FALSE;
    }

    constantValue = &function->constantValueList[constantIndex];
    return import_append_decorator_names_from_constant_array(state, decorators, constantValue);
}

static TZrBool register_runtime_prototypes_from_function(SZrCompilerState *cs, const SZrFunction *function) {
    const TZrByte *currentPos;
    TZrSize remainingDataSize;
    TZrUInt32 prototypeCount;

    if (cs == ZR_NULL || function == ZR_NULL || function->prototypeData == ZR_NULL || function->prototypeDataLength <= sizeof(TZrUInt32) ||
        function->prototypeCount == 0) {
        return ZR_TRUE;
    }

    prototypeCount = function->prototypeCount;
    currentPos = function->prototypeData + sizeof(TZrUInt32);
    remainingDataSize = function->prototypeDataLength - sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < prototypeCount; index++) {
        const SZrCompiledPrototypeInfo *protoInfo;
        TZrSize currentPrototypeSize;
        TZrUInt32 inheritsCount;
        TZrUInt32 membersCount;
        TZrUInt32 decoratorsCount;
        SZrString *prototypeName;
        SZrTypePrototypeInfo typePrototype;

        if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
            return ZR_FALSE;
        }

        protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
        inheritsCount = protoInfo->inheritsCount;
        membersCount = protoInfo->membersCount;
        decoratorsCount = protoInfo->decoratorsCount;
        currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                               inheritsCount * sizeof(TZrUInt32) +
                               decoratorsCount * sizeof(TZrUInt32) +
                               membersCount * sizeof(SZrCompiledMemberInfo);
        if (remainingDataSize < currentPrototypeSize) {
            return ZR_FALSE;
        }

        prototypeName = function_constant_string(cs->state, function, protoInfo->nameStringIndex);
        if (prototypeName != ZR_NULL && find_compiler_type_prototype_inference(cs, prototypeName) == ZR_NULL) {
            const TZrUInt32 *prototypeDecoratorIndices =
                    (const TZrUInt32 *)(currentPos + sizeof(SZrCompiledPrototypeInfo) +
                                        inheritsCount * sizeof(TZrUInt32));
            import_type_prototype_init(cs->state,
                                       &typePrototype,
                                       prototypeName,
                                       (EZrObjectPrototypeType)protoInfo->type);
            typePrototype.accessModifier = (EZrAccessModifier)protoInfo->accessModifier;
            typePrototype.protocolMask = protoInfo->protocolMask;
            if (protoInfo->hasDecoratorMetadata &&
                protoInfo->decoratorMetadataConstantIndex < function->constantValueLength) {
                typePrototype.decoratorMetadataValue = function->constantValueList[protoInfo->decoratorMetadataConstantIndex];
                typePrototype.hasDecoratorMetadata = ZR_TRUE;
            }

            if (inheritsCount > 0) {
                const TZrUInt32 *inheritIndices =
                        (const TZrUInt32 *)(currentPos + sizeof(SZrCompiledPrototypeInfo));
                for (TZrUInt32 inheritIndex = 0; inheritIndex < inheritsCount; inheritIndex++) {
                    SZrString *inheritTypeName =
                            function_constant_string(cs->state, function, inheritIndices[inheritIndex]);
                    if (inheritTypeName != ZR_NULL) {
                        ZrCore_Array_Push(cs->state, &typePrototype.inherits, &inheritTypeName);
                        if (typePrototype.extendsTypeName == ZR_NULL) {
                            typePrototype.extendsTypeName = inheritTypeName;
                        }
                    }
                }
            }
            import_metadata_apply_default_builtin_root(cs->state,
                                                       &typePrototype,
                                                       (EZrObjectPrototypeType)protoInfo->type);

            if (decoratorsCount > 0) {
                for (TZrUInt32 decoratorIndex = 0; decoratorIndex < decoratorsCount; decoratorIndex++) {
                    SZrTypeDecoratorInfo decoratorInfo;
                    SZrString *decoratorName =
                            function_constant_string(cs->state, function, prototypeDecoratorIndices[decoratorIndex]);

                    if (decoratorName == ZR_NULL) {
                        continue;
                    }

                    ZrCore_Memory_RawSet(&decoratorInfo, 0, sizeof(decoratorInfo));
                    decoratorInfo.name = decoratorName;
                    ZrCore_Array_Push(cs->state, &typePrototype.decorators, &decoratorInfo);
                }
            }

            if (membersCount > 0) {
                const SZrCompiledMemberInfo *memberInfos = (const SZrCompiledMemberInfo *)(currentPos +
                                                                                           sizeof(SZrCompiledPrototypeInfo) +
                                                                                           inheritsCount * sizeof(TZrUInt32) +
                                                                                           decoratorsCount * sizeof(TZrUInt32));
                for (TZrUInt32 memberIndex = 0; memberIndex < membersCount; memberIndex++) {
                    const SZrCompiledMemberInfo *compiledMember = &memberInfos[memberIndex];
                    SZrTypeMemberInfo memberInfo;

                    ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
                    memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
                    ZrCore_Array_Construct(&memberInfo.parameterTypes);
                    ZrCore_Array_Construct(&memberInfo.parameterNames);
                    ZrCore_Array_Construct(&memberInfo.parameterHasDefaultValues);
                    ZrCore_Array_Construct(&memberInfo.parameterDefaultValues);
                    ZrCore_Array_Construct(&memberInfo.genericParameters);
                    ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
                    ZrCore_Array_Construct(&memberInfo.decorators);
                    ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
                    memberInfo.memberType = (EZrAstNodeType)compiledMember->memberType;
                    memberInfo.name = function_constant_string(cs->state, function, compiledMember->nameStringIndex);
                    memberInfo.accessModifier = (EZrAccessModifier)compiledMember->accessModifier;
                    memberInfo.isStatic = compiledMember->isStatic ? ZR_TRUE : ZR_FALSE;
                    memberInfo.isConst = compiledMember->isConst ? ZR_TRUE : ZR_FALSE;
                    memberInfo.fieldTypeName =
                            function_constant_string(cs->state, function, compiledMember->fieldTypeNameStringIndex);
                    memberInfo.fieldOffset = compiledMember->fieldOffset;
                    memberInfo.fieldSize = compiledMember->fieldSize;
                    memberInfo.isMetaMethod = compiledMember->isMetaMethod ? ZR_TRUE : ZR_FALSE;
                    memberInfo.metaType = (EZrMetaType)compiledMember->metaType;
                    memberInfo.functionConstantIndex = compiledMember->functionConstantIndex;
                    memberInfo.parameterCount = compiledMember->parameterCount;
                    memberInfo.returnTypeName = function_constant_string(cs->state,
                                                                         function,
                                                                         compiledMember->returnTypeNameStringIndex);
                    memberInfo.isUsingManaged = compiledMember->isUsingManaged ? ZR_TRUE : ZR_FALSE;
                    memberInfo.ownershipQualifier = (EZrOwnershipQualifier)compiledMember->ownershipQualifier;
                    memberInfo.callsClose = compiledMember->callsClose ? ZR_TRUE : ZR_FALSE;
                    memberInfo.callsDestructor = compiledMember->callsDestructor ? ZR_TRUE : ZR_FALSE;
                    memberInfo.declarationOrder = compiledMember->declarationOrder;
                    memberInfo.contractRole = compiledMember->contractRole;
                    if (compiledMember->hasDecoratorMetadata &&
                        compiledMember->decoratorMetadataConstantIndex < function->constantValueLength) {
                        memberInfo.decoratorMetadataValue =
                                function->constantValueList[compiledMember->decoratorMetadataConstantIndex];
                        memberInfo.hasDecoratorMetadata = ZR_TRUE;
                    }
                    if (compiledMember->hasDecoratorNames) {
                        import_append_member_decorator_names_from_function_constant(cs->state,
                                                                                   &memberInfo.decorators,
                                                                                   function,
                                                                                   compiledMember->decoratorNamesConstantIndex);
                    }
                    {
                        const SZrFunction *metadataFunction =
                                resolve_runtime_member_metadata_function(cs->state,
                                                                         function,
                                                                         compiledMember,
                                                                         &memberInfo);
                        if (metadataFunction != ZR_NULL) {
                            import_copy_runtime_parameter_metadata(cs, &memberInfo, metadataFunction);
                        }
                    }
                    ZrCore_Array_Push(cs->state, &typePrototype.members, &memberInfo);
                }
            }

            register_imported_type_name(cs, prototypeName);
            ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
        }

        currentPos += currentPrototypeSize;
        remainingDataSize -= currentPrototypeSize;
    }

    return ZR_TRUE;
}

static TZrBool register_runtime_import_metadata(SZrCompilerState *cs,
                                                SZrString *moduleName,
                                                const SZrFunction *function) {
    SZrTypePrototypeInfo modulePrototype;
    TZrSize initialTypeCount;

    if (cs == ZR_NULL || moduleName == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    import_type_prototype_init(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    import_metadata_apply_default_builtin_root(cs->state, &modulePrototype, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    import_metadata_apply_default_builtin_root(cs->state, &modulePrototype, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    initialTypeCount = cs->typePrototypes.length;
    if (!register_runtime_prototypes_from_function(cs, function)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        const SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            import_add_function_member_from_symbol(cs, &modulePrototype, symbol, function);
        } else {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    symbol->name,
                                    typed_type_ref_to_type_name(cs, &symbol->valueType),
                                    ZR_TRUE,
                                    (EZrModuleExportKind)symbol->exportKind,
                                    (EZrModuleExportReadiness)symbol->readiness);
        }
    }

    for (TZrSize index = initialTypeCount; index < cs->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *info = (SZrTypePrototypeInfo *)ZrCore_Array_Get(&cs->typePrototypes, index);
        if (info != ZR_NULL && info->name != ZR_NULL && info->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    info->name,
                                    info->name,
                                    ZR_TRUE,
                                    ZR_MODULE_EXPORT_KIND_TYPE,
                                    ZR_MODULE_EXPORT_READY_DECLARATION);
        }
    }

    register_imported_type_name(cs, moduleName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

static void register_io_type_prototype_stub(SZrCompilerState *cs,
                                            SZrTypePrototypeInfo *modulePrototype,
                                            SZrString *typeName,
                                            EZrObjectPrototypeType type) {
    SZrTypePrototypeInfo typePrototype;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || typeName == ZR_NULL ||
        find_compiler_type_prototype_inference(cs, typeName) != ZR_NULL) {
        return;
    }

    import_type_prototype_init(cs->state, &typePrototype, typeName, type);
    import_metadata_apply_default_builtin_root(cs->state, &typePrototype, type);
    register_imported_type_name(cs, typeName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
    import_add_field_member(cs->state,
                            modulePrototype,
                            typeName,
                            typeName,
                            ZR_TRUE,
                            ZR_MODULE_EXPORT_KIND_TYPE,
                            ZR_MODULE_EXPORT_READY_DECLARATION);
}

static TZrBool register_binary_import_metadata(SZrCompilerState *cs,
                                               SZrString *moduleName,
                                               const SZrIoFunction *function) {
    SZrTypePrototypeInfo modulePrototype;

    if (cs == ZR_NULL || moduleName == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    import_type_prototype_init(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    import_metadata_apply_default_builtin_root(cs->state, &modulePrototype, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    for (TZrSize index = 0; index < function->typedExportedSymbolsLength; index++) {
        const SZrIoFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];

        if (symbol->name == ZR_NULL) {
            continue;
        }

        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            import_add_function_member_from_io_symbol(cs, &modulePrototype, symbol, function);
        } else {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    symbol->name,
                                    io_typed_type_ref_to_type_name(cs, &symbol->valueType),
                                    ZR_TRUE,
                                    (EZrModuleExportKind)symbol->exportKind,
                                    (EZrModuleExportReadiness)symbol->readiness);
        }
    }

    if (function->classes != ZR_NULL) {
        for (TZrSize index = 0; index < function->classesLength; index++) {
            register_io_type_prototype_stub(cs,
                                            &modulePrototype,
                                            function->classes[index].name,
                                            ZR_OBJECT_PROTOTYPE_TYPE_CLASS);
        }
    }
    if (function->structs != ZR_NULL) {
        for (TZrSize index = 0; index < function->structsLength; index++) {
            register_io_type_prototype_stub(cs,
                                            &modulePrototype,
                                            function->structs[index].name,
                                            ZR_OBJECT_PROTOTYPE_TYPE_STRUCT);
        }
    }

    register_imported_type_name(cs, moduleName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

static void register_summary_type_prototype_stub(SZrCompilerState *cs,
                                                 SZrTypePrototypeInfo *modulePrototype,
                                                 const SZrModuleInitExportInfo *exportInfo) {
    SZrTypePrototypeInfo typePrototype;

    if (cs == ZR_NULL || modulePrototype == ZR_NULL || exportInfo == ZR_NULL || exportInfo->name == ZR_NULL ||
        find_compiler_type_prototype_inference(cs, exportInfo->name) != ZR_NULL) {
        return;
    }

    import_type_prototype_init(cs->state,
                               &typePrototype,
                               exportInfo->name,
                               (EZrObjectPrototypeType)exportInfo->prototypeType);
    import_metadata_apply_default_builtin_root(cs->state,
                                               &typePrototype,
                                               (EZrObjectPrototypeType)exportInfo->prototypeType);
    register_imported_type_name(cs, exportInfo->name);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &typePrototype);
    import_add_field_member(cs->state,
                            modulePrototype,
                            exportInfo->name,
                            exportInfo->name,
                            ZR_TRUE,
                            ZR_MODULE_EXPORT_KIND_TYPE,
                            ZR_MODULE_EXPORT_READY_DECLARATION);
}

static TZrBool register_summary_import_metadata(SZrCompilerState *cs,
                                                SZrString *moduleName,
                                                const SZrParserModuleInitSummary *summary) {
    SZrTypePrototypeInfo modulePrototype;
    TZrSize index;

    if (cs == ZR_NULL || moduleName == ZR_NULL || summary == ZR_NULL) {
        return ZR_FALSE;
    }

    import_type_prototype_init(cs->state, &modulePrototype, moduleName, ZR_OBJECT_PROTOTYPE_TYPE_MODULE);
    for (index = 0; index < summary->exports.length; ++index) {
        const SZrModuleInitExportInfo *exportInfo =
                (const SZrModuleInitExportInfo *)ZrCore_Array_Get((SZrArray *)&summary->exports, index);

        if (exportInfo == ZR_NULL || exportInfo->name == ZR_NULL) {
            continue;
        }

        if (exportInfo->exportKind == ZR_MODULE_EXPORT_KIND_FUNCTION) {
            SZrTypeMemberInfo memberInfo;
            ZrCore_Memory_RawSet(&memberInfo, 0, sizeof(memberInfo));
            memberInfo.minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
            ZrCore_Array_Construct(&memberInfo.parameterTypes);
            ZrCore_Array_Construct(&memberInfo.parameterNames);
            ZrCore_Array_Construct(&memberInfo.parameterHasDefaultValues);
            ZrCore_Array_Construct(&memberInfo.parameterDefaultValues);
            ZrCore_Array_Construct(&memberInfo.genericParameters);
            ZrCore_Array_Construct(&memberInfo.parameterPassingModes);
            ZrCore_Array_Construct(&memberInfo.decorators);
            ZrCore_Value_ResetAsNull(&memberInfo.decoratorMetadataValue);
            memberInfo.memberType = ZR_AST_CLASS_METHOD;
            memberInfo.name = exportInfo->name;
            memberInfo.accessModifier = (EZrAccessModifier)exportInfo->accessModifier;
            memberInfo.isStatic = ZR_TRUE;
            memberInfo.parameterCount = exportInfo->parameterCount;
            memberInfo.returnTypeName = typed_type_ref_to_type_name(cs, &exportInfo->valueType);
            memberInfo.moduleExportKind = ZR_MODULE_EXPORT_KIND_FUNCTION;
            memberInfo.moduleExportReadiness = ZR_MODULE_EXPORT_READY_DECLARATION;
            if (exportInfo->parameterCount > 0 && exportInfo->parameterTypes != ZR_NULL) {
                ZrCore_Array_Init(cs->state,
                                  &memberInfo.parameterTypes,
                                  sizeof(SZrInferredType),
                                  exportInfo->parameterCount);
                for (TZrUInt32 paramIndex = 0; paramIndex < exportInfo->parameterCount; ++paramIndex) {
                    SZrInferredType paramType;
                    ZrParser_InferredType_Init(cs->state, &paramType, ZR_VALUE_TYPE_OBJECT);
                    function_typed_type_ref_to_inferred(cs, &exportInfo->parameterTypes[paramIndex], &paramType);
                    ZrCore_Array_Push(cs->state, &memberInfo.parameterTypes, &paramType);
                }
            }
            ZrCore_Array_Push(cs->state, &modulePrototype.members, &memberInfo);
        } else if (exportInfo->exportKind == ZR_MODULE_EXPORT_KIND_TYPE) {
            register_summary_type_prototype_stub(cs, &modulePrototype, exportInfo);
        } else {
            import_add_field_member(cs->state,
                                    &modulePrototype,
                                    exportInfo->name,
                                    typed_type_ref_to_type_name(cs, &exportInfo->valueType),
                                    ZR_TRUE,
                                    (EZrModuleExportKind)exportInfo->exportKind,
                                    (EZrModuleExportReadiness)exportInfo->readiness);
        }
    }

    register_imported_type_name(cs, moduleName);
    ZrCore_Array_Push(cs->state, &cs->typePrototypes, &modulePrototype);
    return ZR_TRUE;
}

TZrBool ensure_import_module_compile_info(SZrCompilerState *cs, SZrString *moduleName) {
    SZrGlobalState *global;
    TZrNativeString moduleNameText;
    TZrSize moduleNameLength;
    SZrIo io;
    TZrBool loaderSuccess;
    TZrBool pushedImportStack = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    import_metadata_trace("ensure import compile info enter module=%p", (void *)moduleName);

    if (find_registered_type_prototype_inference_exact_only(cs, moduleName) != ZR_NULL) {
        import_metadata_trace("ensure import compile info hit registered prototype");
        return ZR_TRUE;
    }

    if (ensure_native_module_compile_info(cs, moduleName)) {
        import_metadata_trace("ensure import compile info satisfied by native module");
        return ZR_TRUE;
    }

    if (import_compile_info_stack_contains(cs->state->global, moduleName)) {
        const SZrParserModuleInitSummary *summary;
        if (!ZrParser_ModuleInitAnalysis_EnsureSummary(cs, moduleName)) {
            import_metadata_trace("ensure import compile info cyclic summary ensure failed");
            report_import_compile_info_failure(cs, moduleName);
            return ZR_FALSE;
        }
        summary = ZrParser_ModuleInitAnalysis_FindSummary(cs->state->global, moduleName);
        if (summary == ZR_NULL) {
            import_metadata_trace("ensure import compile info cyclic summary missing");
            report_import_compile_info_failure(cs, moduleName);
            return ZR_FALSE;
        }
        result = register_summary_import_metadata(cs, moduleName, summary);
        import_metadata_trace("ensure import compile info cyclic summary register result=%d", (int)result);
        if (!result) {
            report_import_compile_info_failure(cs, moduleName);
        }
        return result;
    }

    ZrCore_Array_Push(cs->state, &cs->state->global->importCompileInfoStack, &moduleName);
    pushedImportStack = ZR_TRUE;

    global = cs->state->global;
    if (global->sourceLoader == ZR_NULL) {
        goto cleanup;
    }

    moduleNameText = import_metadata_string_text(moduleName);
    if (moduleNameText == ZR_NULL) {
        import_metadata_trace("ensure import compile info module text missing");
        goto cleanup;
    }
    import_metadata_trace("ensure import compile info module='%s'", moduleNameText);
    moduleNameLength = strlen(moduleNameText);
    ZrCore_Io_Init(cs->state, &io, ZR_NULL, ZR_NULL, ZR_NULL);
    loaderSuccess = global->sourceLoader(cs->state, moduleNameText, ZR_NULL, &io);
    import_metadata_trace("ensure import compile info sourceLoader result=%d isBinary=%d",
                          (int)loaderSuccess,
                          (int)io.isBinary);
    if (!loaderSuccess) {
        goto cleanup;
    }

    if (io.isBinary) {
        SZrIoSource *source = ZR_NULL;

        if (!ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(cs->state, &io, &source)) {
            if (io.close != ZR_NULL) {
                io.close(cs->state, io.customData);
            }
            goto cleanup;
        }

        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }

        if (source != ZR_NULL &&
            source->modulesLength > 0 &&
            source->modules != ZR_NULL &&
            source->modules[0].entryFunction != ZR_NULL) {
            result = register_binary_import_metadata(cs, moduleName, source->modules[0].entryFunction);
            import_metadata_trace("ensure import compile info register binary result=%d", (int)result);
        }

        if (source != ZR_NULL) {
            ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(cs->state->global, source);
        }

        goto cleanup;
    }

    if (global->compileSource != ZR_NULL) {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_import_bytes(cs->state, &io, &sourceSize);
        SZrFunction *compiledFunction = ZR_NULL;

        if (io.close != ZR_NULL) {
            io.close(cs->state, io.customData);
        }

        if (sourceBuffer == ZR_NULL || sourceSize == 0) {
            import_metadata_trace("ensure import compile info read source failed size=%llu",
                                  (unsigned long long)sourceSize);
            if (sourceBuffer != ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global,
                                              sourceBuffer,
                                              sourceSize + 1,
                                              ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            }
            goto cleanup;
        }

        import_metadata_trace("ensure import compile info compileSource start module='%s' size=%llu",
                              moduleNameText,
                              (unsigned long long)sourceSize);
        compiledFunction = global->compileSource(cs->state,
                                                 (const TZrChar *)sourceBuffer,
                                                 sourceSize,
                                                 ZrCore_String_Create(cs->state, moduleNameText, moduleNameLength));
        ZrCore_Memory_RawFreeWithType(global,
                                      sourceBuffer,
                                      sourceSize + 1,
                                      ZR_MEMORY_NATIVE_TYPE_GLOBAL);
        if (compiledFunction == ZR_NULL) {
            import_metadata_trace("ensure import compile info compileSource returned null");
            goto cleanup;
        }

        result = register_runtime_import_metadata(cs, moduleName, compiledFunction);
        import_metadata_trace("ensure import compile info register runtime result=%d func=%p",
                              (int)result,
                              (void *)compiledFunction);
        ZrCore_Function_Free(cs->state, compiledFunction);
        goto cleanup;
    }

    if (io.close != ZR_NULL) {
        io.close(cs->state, io.customData);
    }

cleanup:
    if (pushedImportStack && cs->state->global != ZR_NULL && cs->state->global->importCompileInfoStack.length > 0) {
        cs->state->global->importCompileInfoStack.length--;
    }
    if (!result) {
        report_import_compile_info_failure(cs, moduleName);
    }
    import_metadata_trace("ensure import compile info exit result=%d", (int)result);
    return result;
}

static TZrBool import_metadata_trace_enabled(void) {
    static TZrBool initialized = ZR_FALSE;
    static TZrBool enabled = ZR_FALSE;

    if (!initialized) {
        const TZrChar *flag = getenv("ZR_VM_TRACE_PROJECT_STARTUP");
        enabled = (flag != ZR_NULL && flag[0] != '\0') ? ZR_TRUE : ZR_FALSE;
        initialized = ZR_TRUE;
    }

    return enabled;
}

static void import_metadata_trace(const TZrChar *format, ...) {
    va_list arguments;

    if (!import_metadata_trace_enabled() || format == ZR_NULL) {
        return;
    }

    va_start(arguments, format);
    fprintf(stderr, "[zr-import-meta] ");
    vfprintf(stderr, format, arguments);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(arguments);
}
