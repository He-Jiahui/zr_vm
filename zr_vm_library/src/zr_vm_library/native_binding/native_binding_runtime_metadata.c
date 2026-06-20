#include "native_binding/native_binding_internal.h"

#include "zr_vm_core/hash.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_common/zr_ast_constants.h"

static const TZrByte CZrNativeRuntimeMetadataSignatureHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        'n',
        'a',
        't',
        'i',
        'v',
        'e',
        '.',
        's',
        'i',
        'g',
        '.',
        'v',
        '1',
        '\0',
};

#define ZR_NATIVE_RUNTIME_METADATA_MEMBER_PARAMETER_COUNT_UNKNOWN ((TZrUInt32)-1)

static TZrSize native_runtime_metadata_string_length(const TZrChar *value) {
    return value != ZR_NULL ? strlen(value) : 0;
}

static void native_runtime_metadata_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value) {
    buffer[*offset + 0] = (TZrByte)(value & 0xFFu);
    buffer[*offset + 1] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[*offset + 2] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[*offset + 3] = (TZrByte)((value >> 24) & 0xFFu);
    *offset += 4u;
}

static void native_runtime_metadata_write_string(TZrByte *buffer, TZrSize *offset, const TZrChar *value) {
    TZrSize length = native_runtime_metadata_string_length(value);

    native_runtime_metadata_write_u32(buffer, offset, (TZrUInt32)length);
    if (length == 0u) {
        return;
    }

    ZrCore_Memory_RawCopy(buffer + *offset, (TZrPtr)value, length);
    *offset += length;
}

static TZrSize native_runtime_metadata_parameter_type_signature_size(
        const ZrLibParameterDescriptor *parameters,
        TZrSize parameterCount) {
    TZrSize size = sizeof(TZrUInt32);

    if (parameters == ZR_NULL) {
        return size;
    }

    for (TZrSize index = 0; index < parameterCount; index++) {
        size += sizeof(TZrUInt32) + native_runtime_metadata_string_length(parameters[index].typeName);
    }
    return size;
}

static void native_runtime_metadata_write_parameter_type_signature(
        TZrByte *buffer,
        TZrSize *offset,
        const ZrLibParameterDescriptor *parameters,
        TZrSize parameterCount) {
    TZrUInt32 parameterLength = parameters != ZR_NULL ? (TZrUInt32)parameterCount : 0u;

    native_runtime_metadata_write_u32(buffer, offset, parameterLength);
    for (TZrUInt32 index = 0; index < parameterLength; index++) {
        native_runtime_metadata_write_string(buffer, offset, parameters[index].typeName);
    }
}

static TZrSize native_runtime_metadata_member_signature_size(
        EZrMetadataSignatureNode signatureNode,
        TZrUInt32 memberType,
        const TZrChar *ownerName,
        const TZrChar *memberName,
        const TZrChar *valueTypeName,
        const ZrLibParameterDescriptor *parameters,
        TZrSize parameterTypeCount) {
    TZrSize size = 0;

    size += sizeof(TZrUInt32);
    size += sizeof(TZrUInt32);
    size += sizeof(TZrUInt32) + native_runtime_metadata_string_length(ownerName);
    size += sizeof(TZrUInt32) + native_runtime_metadata_string_length(memberName);
    size += sizeof(TZrUInt32) + native_runtime_metadata_string_length(valueTypeName);
    if (signatureNode == ZR_METADATA_SIGNATURE_NODE_METHOD_SIG) {
        size += sizeof(TZrUInt32);
        size += sizeof(TZrUInt32);
        size += native_runtime_metadata_parameter_type_signature_size(parameters, parameterTypeCount);
    }

    return size;
}

static TZrUInt64 native_runtime_metadata_member_signature_hash(
        SZrState *state,
        EZrMetadataSignatureNode signatureNode,
        TZrUInt32 memberType,
        const TZrChar *ownerName,
        const TZrChar *memberName,
        const TZrChar *valueTypeName,
        TZrUInt32 parameterCount,
        TZrUInt32 minArgumentCount,
        const ZrLibParameterDescriptor *parameters,
        TZrSize parameterTypeCount) {
    TZrSize signatureSize;
    TZrByte *signatureBlob;
    TZrSize offset = 0;
    TZrUInt64 signatureHash;

    if (state == ZR_NULL || state->global == ZR_NULL || memberName == ZR_NULL) {
        return 0;
    }

    signatureSize = native_runtime_metadata_member_signature_size(signatureNode,
                                                                 memberType,
                                                                 ownerName,
                                                                 memberName,
                                                                 valueTypeName,
                                                                 parameters,
                                                                 parameterTypeCount);
    if (signatureSize == 0u) {
        return 0;
    }

    signatureBlob = (TZrByte *)ZrCore_Memory_RawMallocWithType(state->global,
                                                              signatureSize,
                                                              ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    if (signatureBlob == ZR_NULL) {
        return 0;
    }

    native_runtime_metadata_write_u32(signatureBlob, &offset, (TZrUInt32)signatureNode);
    native_runtime_metadata_write_u32(signatureBlob, &offset, memberType);
    native_runtime_metadata_write_string(signatureBlob, &offset, ownerName);
    native_runtime_metadata_write_string(signatureBlob, &offset, memberName);
    native_runtime_metadata_write_string(signatureBlob, &offset, valueTypeName);
    if (signatureNode == ZR_METADATA_SIGNATURE_NODE_METHOD_SIG) {
        native_runtime_metadata_write_u32(signatureBlob, &offset, parameterCount);
        native_runtime_metadata_write_u32(signatureBlob, &offset, minArgumentCount);
        native_runtime_metadata_write_parameter_type_signature(signatureBlob,
                                                              &offset,
                                                              parameters,
                                                              parameterTypeCount);
    }

    signatureHash = ZrCore_Hash_CreateStable64WithPrefix(CZrNativeRuntimeMetadataSignatureHashV1Prefix,
                                                         sizeof(CZrNativeRuntimeMetadataSignatureHashV1Prefix),
                                                         signatureBlob,
                                                         offset);
    ZrCore_Memory_RawFreeWithType(state->global,
                                  signatureBlob,
                                  signatureSize,
                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
    return signatureHash;
}

static TZrBool native_runtime_metadata_primitive_type(const TZrChar *typeName, EZrValueType *outType) {
    TZrSize length;

    if (typeName == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(typeName);
    if (length == 3u && memcmp(typeName, "int", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (length == 4u && memcmp(typeName, "uint", 4u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (length == 5u && memcmp(typeName, "float", 5u) == 0) {
        *outType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }
    if (length == 4u && memcmp(typeName, "bool", 4u) == 0) {
        *outType = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (length == 6u && memcmp(typeName, "string", 6u) == 0) {
        *outType = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }
    if ((length == 4u && memcmp(typeName, "null", 4u) == 0) ||
        (length == 4u && memcmp(typeName, "void", 4u) == 0)) {
        *outType = ZR_VALUE_TYPE_NULL;
        return ZR_TRUE;
    }
    if (length == 2u && memcmp(typeName, "i8", 2u) == 0) {
        *outType = ZR_VALUE_TYPE_INT8;
        return ZR_TRUE;
    }
    if (length == 2u && memcmp(typeName, "u8", 2u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT8;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "i16", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT16;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "u16", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT16;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "i32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT32;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "u32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT32;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "i64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_INT64;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "u64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_UINT64;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "f32", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_FLOAT;
        return ZR_TRUE;
    }
    if (length == 3u && memcmp(typeName, "f64", 3u) == 0) {
        *outType = ZR_VALUE_TYPE_DOUBLE;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void native_runtime_metadata_set_type_ref(SZrState *state,
                                                 SZrFunctionTypedTypeRef *outType,
                                                 const TZrChar *typeName) {
    EZrValueType primitiveType;

    if (outType == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outType, 0, sizeof(*outType));
    outType->baseType = ZR_VALUE_TYPE_OBJECT;
    outType->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    if (native_runtime_metadata_primitive_type(typeName, &primitiveType)) {
        outType->baseType = primitiveType;
        return;
    }
    outType->typeName = native_binding_create_string(state, typeName);
}

static TZrUInt32 native_runtime_metadata_function_parameter_count(const ZrLibFunctionDescriptor *descriptor) {
    if (descriptor == ZR_NULL) {
        return ZR_NATIVE_RUNTIME_METADATA_MEMBER_PARAMETER_COUNT_UNKNOWN;
    }
    if (descriptor->parameters != ZR_NULL ||
        (descriptor->minArgumentCount == 0u && descriptor->maxArgumentCount == 0u)) {
        return (TZrUInt32)descriptor->parameterCount;
    }
    if (descriptor->minArgumentCount == descriptor->maxArgumentCount) {
        return descriptor->minArgumentCount;
    }
    return ZR_NATIVE_RUNTIME_METADATA_MEMBER_PARAMETER_COUNT_UNKNOWN;
}

static void native_runtime_metadata_init_symbol(SZrState *state,
                                                SZrFunctionTypedExportSymbol *symbol,
                                                const TZrChar *name,
                                                TZrUInt8 symbolKind,
                                                TZrUInt8 exportKind,
                                                TZrUInt8 readiness,
                                                const TZrChar *valueTypeName,
                                                TZrUInt32 parameterCount,
                                                TZrMetadataToken metadataToken,
                                                TZrMetadataToken signatureToken,
                                                TZrUInt64 signatureHash) {
    if (symbol == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(symbol, 0, sizeof(*symbol));
    symbol->name = native_binding_create_string(state, name);
    symbol->accessModifier = ZR_ACCESS_CONSTANT_PUBLIC;
    symbol->symbolKind = symbolKind;
    symbol->exportKind = exportKind;
    symbol->readiness = readiness;
    symbol->callableChildIndex = ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE;
    native_runtime_metadata_set_type_ref(state, &symbol->valueType, valueTypeName);
    symbol->parameterCount = parameterCount;
    symbol->metadataToken = metadataToken;
    symbol->signatureToken = signatureToken;
    symbol->signatureHash = signatureHash;
}

static TZrBool native_runtime_metadata_add_function_symbol(SZrState *state,
                                                           SZrFunctionTypedExportSymbol *symbol,
                                                           const ZrLibModuleDescriptor *moduleDescriptor,
                                                           const ZrLibFunctionDescriptor *functionDescriptor,
                                                           TZrUInt32 *memberRidCursor,
                                                           TZrUInt32 *signatureRidCursor) {
    TZrUInt32 parameterCount;
    TZrUInt64 signatureHash;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;
    TZrSize parameterTypeCount;

    if (state == ZR_NULL || symbol == ZR_NULL || moduleDescriptor == ZR_NULL ||
        functionDescriptor == ZR_NULL || functionDescriptor->name == ZR_NULL ||
        memberRidCursor == ZR_NULL || signatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = native_runtime_metadata_function_parameter_count(functionDescriptor);
    parameterTypeCount = functionDescriptor->parameters != ZR_NULL ||
                                 (functionDescriptor->minArgumentCount == 0u &&
                                  functionDescriptor->maxArgumentCount == 0u)
                         ? functionDescriptor->parameterCount
                         : 0u;
    signatureHash = native_runtime_metadata_member_signature_hash(
            state,
            ZR_METADATA_SIGNATURE_NODE_METHOD_SIG,
            ZR_AST_CONSTANT_CLASS_METHOD,
            moduleDescriptor->moduleName,
            functionDescriptor->name,
            functionDescriptor->returnTypeName,
            parameterCount,
            functionDescriptor->minArgumentCount,
            functionDescriptor->parameters,
            parameterTypeCount);
    if (signatureHash == 0u) {
        return ZR_FALSE;
    }

    metadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, (*memberRidCursor)++);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*signatureRidCursor)++);
    native_runtime_metadata_init_symbol(state,
                                        symbol,
                                        functionDescriptor->name,
                                        ZR_FUNCTION_TYPED_SYMBOL_FUNCTION,
                                        ZR_MODULE_EXPORT_KIND_FUNCTION,
                                        ZR_MODULE_EXPORT_READY_ENTRY,
                                        functionDescriptor->returnTypeName,
                                        parameterCount,
                                        metadataToken,
                                        signatureToken,
                                        signatureHash);
    return symbol->name != ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

static TZrBool native_runtime_metadata_add_field_symbol(SZrState *state,
                                                        SZrFunctionTypedExportSymbol *symbol,
                                                        const TZrChar *moduleName,
                                                        const TZrChar *name,
                                                        const TZrChar *typeName,
                                                        TZrUInt8 exportKind,
                                                        TZrUInt32 *memberRidCursor,
                                                        TZrUInt32 *signatureRidCursor) {
    TZrUInt64 signatureHash;
    TZrMetadataToken metadataToken;
    TZrMetadataToken signatureToken;

    if (state == ZR_NULL || symbol == ZR_NULL || moduleName == ZR_NULL ||
        name == ZR_NULL || memberRidCursor == ZR_NULL || signatureRidCursor == ZR_NULL) {
        return ZR_FALSE;
    }

    signatureHash = native_runtime_metadata_member_signature_hash(state,
                                                                 ZR_METADATA_SIGNATURE_NODE_FIELD_SIG,
                                                                 ZR_AST_CONSTANT_CLASS_FIELD,
                                                                 moduleName,
                                                                 name,
                                                                 typeName,
                                                                 ZR_NATIVE_RUNTIME_METADATA_MEMBER_PARAMETER_COUNT_UNKNOWN,
                                                                 ZR_NATIVE_RUNTIME_METADATA_MEMBER_PARAMETER_COUNT_UNKNOWN,
                                                                 ZR_NULL,
                                                                 0u);
    if (signatureHash == 0u) {
        return ZR_FALSE;
    }

    metadataToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, (*memberRidCursor)++);
    signatureToken = ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, (*signatureRidCursor)++);
    native_runtime_metadata_init_symbol(state,
                                        symbol,
                                        name,
                                        ZR_FUNCTION_TYPED_SYMBOL_VARIABLE,
                                        exportKind,
                                        ZR_MODULE_EXPORT_READY_ENTRY,
                                        typeName,
                                        0u,
                                        metadataToken,
                                        signatureToken,
                                        signatureHash);
    return symbol->name != ZR_NULL ? ZR_TRUE : ZR_FALSE;
}

TZrBool native_runtime_metadata_attach_module(SZrState *state,
                                              SZrObjectModule *module,
                                              const ZrLibModuleDescriptor *descriptor) {
    SZrFunction *metadataFunction;
    TZrSize symbolCount;
    TZrSize symbolIndex = 0;
    TZrUInt32 memberRidCursor = 1u;
    TZrUInt32 signatureRidCursor = 1u;

    if (state == ZR_NULL || state->global == ZR_NULL || module == ZR_NULL ||
        descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    symbolCount = descriptor->functionCount +
                  descriptor->constantCount +
                  descriptor->moduleLinkCount +
                  descriptor->typeCount;
    metadataFunction = ZrCore_Function_New(state);
    if (metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    metadataFunction->functionName = native_binding_create_string(state, descriptor->moduleName);
    metadataFunction->moduleVersion = native_binding_create_string(
            state,
            descriptor->moduleVersion != ZR_NULL ? descriptor->moduleVersion : "1.0.0");
    if (metadataFunction->functionName == ZR_NULL || metadataFunction->moduleVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbolCount > 0u) {
        metadataFunction->typedExportedSymbols =
                (SZrFunctionTypedExportSymbol *)ZrCore_Memory_RawMallocWithType(
                        state->global,
                        sizeof(SZrFunctionTypedExportSymbol) * symbolCount,
                        ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (metadataFunction->typedExportedSymbols == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrCore_Memory_RawSet(metadataFunction->typedExportedSymbols,
                             0,
                             sizeof(SZrFunctionTypedExportSymbol) * symbolCount);
        metadataFunction->typedExportedSymbolLength = (TZrUInt32)symbolCount;
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        if (!native_runtime_metadata_add_function_symbol(state,
                                                         &metadataFunction->typedExportedSymbols[symbolIndex++],
                                                         descriptor,
                                                         &descriptor->functions[index],
                                                         &memberRidCursor,
                                                         &signatureRidCursor)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constant = &descriptor->constants[index];
        if (!native_runtime_metadata_add_field_symbol(state,
                                                      &metadataFunction->typedExportedSymbols[symbolIndex++],
                                                      descriptor->moduleName,
                                                      constant->name,
                                                      native_metadata_constant_type_name(constant),
                                                      ZR_MODULE_EXPORT_KIND_VALUE,
                                                      &memberRidCursor,
                                                      &signatureRidCursor)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &descriptor->moduleLinks[index];
        if (!native_runtime_metadata_add_field_symbol(state,
                                                      &metadataFunction->typedExportedSymbols[symbolIndex++],
                                                      descriptor->moduleName,
                                                      link->name,
                                                      link->moduleName,
                                                      ZR_MODULE_EXPORT_KIND_VALUE,
                                                      &memberRidCursor,
                                                      &signatureRidCursor)) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *type = &descriptor->types[index];
        if (!native_runtime_metadata_add_field_symbol(state,
                                                      &metadataFunction->typedExportedSymbols[symbolIndex++],
                                                      descriptor->moduleName,
                                                      type->name,
                                                      type->name,
                                                      ZR_MODULE_EXPORT_KIND_TYPE,
                                                      &memberRidCursor,
                                                      &signatureRidCursor)) {
            return ZR_FALSE;
        }
    }

    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, module, metadataFunction);
    return ZR_TRUE;
}
