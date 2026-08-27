#include "compiler_internal.h"
#include "compiler_ffi_callable_contract.h"

#include "zr_vm_parser/ffi_contract.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_core/string.h"

#include <string.h>

#define ZR_FFI_CONTRACT_MAX_TYPE_DEPTH ((TZrUInt32)16u)

static void ffi_contract_set_diagnostic(
        SZrFfiContractDiagnostic *diagnostic,
        EZrFfiContractStatus status,
        TZrUInt32 parameterIndex,
        SZrFileRange sourceRange) {
    if (diagnostic == ZR_NULL) {
        return;
    }
    diagnostic->status = status;
    diagnostic->parameterIndex = parameterIndex;
    diagnostic->sourceRange = sourceRange;
}

static TZrUInt64 ffi_contract_hash_bytes(
        TZrUInt64 hash,
        const void *data,
        TZrSize size) {
    const TZrByte *bytes = (const TZrByte *)data;

    for (TZrSize index = 0u; index < size; index++) {
        hash = ZrCommon_FfiContract_HashByte(hash, bytes[index]);
    }
    return hash;
}

static TZrUInt64 ffi_contract_hash_text(
        TZrUInt64 hash,
        const TZrChar *text) {
    return ffi_contract_hash_bytes(
            hash,
            text != ZR_NULL ? text : "",
            text != ZR_NULL ? strlen(text) + 1u : 1u);
}

static EZrFfiContractStatus ffi_contract_copy_text(
        TZrChar *destination,
        TZrSize capacity,
        const TZrChar *source,
        EZrFfiContractStatus limitStatus) {
    TZrSize length;

    if (destination == ZR_NULL || capacity == 0u || source == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    length = strlen(source);
    if (length >= capacity) {
        return limitStatus;
    }
    memcpy(destination, source, length + 1u);
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_build_import_metadata(
        const SZrExternFunctionDeclaration *function,
        SZrFileRange sourceRange,
        SZrNativeImportContract *contract) {
    SZrString *platformValue;
    const TZrChar *platformText;
    const TZrChar *sourceDocument = "<unknown>";
    TZrInt64 requiredCapabilities =
            (TZrInt64)ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME;
    EZrFfiContractStatus status;

    if (function == ZR_NULL || contract == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_ALL;
    platformValue = extern_compiler_decorators_get_string_arg(
            function->decorators, "platform");
    if (platformValue != ZR_NULL) {
        platformText = ZrCore_String_GetNativeString(platformValue);
        if (platformText != ZR_NULL && strcmp(platformText, "any") == 0) {
            contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_ALL;
        } else if (platformText != ZR_NULL &&
                   strcmp(platformText, "windows") == 0) {
            contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_WINDOWS;
        } else if (platformText != ZR_NULL && strcmp(platformText, "unix") == 0) {
            contract->availability = ZR_FFI_CONTRACT_AVAILABILITY_UNIX;
        } else {
            return ZR_FFI_CONTRACT_STATUS_INVALID_POLICY;
        }
    }
    if (extern_compiler_decorators_get_int_arg(
                function->decorators,
                "requiredCapabilities",
                &requiredCapabilities) &&
        (requiredCapabilities < 0 ||
         (((TZrUInt64)requiredCapabilities &
           ZR_FFI_CONTRACT_CAPABILITY_FFI_RUNTIME) == 0u))) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_POLICY;
    }
    contract->requiredCapabilities = (TZrUInt64)requiredCapabilities;

    if (sourceRange.source != ZR_NULL) {
        sourceDocument = ZrCore_String_GetNativeString(sourceRange.source);
    }
    status = ffi_contract_copy_text(
            contract->sourceMapping.document,
            ZR_FFI_CONTRACT_SOURCE_DOCUMENT_CAPACITY,
            sourceDocument != ZR_NULL ? sourceDocument : "<unknown>",
            ZR_FFI_CONTRACT_STATUS_SOURCE_DOCUMENT_LIMIT);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    contract->sourceMapping.startOffset = (TZrUInt64)sourceRange.start.offset;
    contract->sourceMapping.endOffset = (TZrUInt64)sourceRange.end.offset;
    contract->sourceMapping.startLine = sourceRange.start.line;
    contract->sourceMapping.startColumn = sourceRange.start.column;
    contract->sourceMapping.endLine = sourceRange.end.line;
    contract->sourceMapping.endColumn = sourceRange.end.column;
    contract->declaringModuleId = ffi_contract_hash_text(
            ZR_FFI_CONTRACT_FNV_OFFSET,
            contract->sourceMapping.document);
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static TZrUInt32 ffi_contract_align_up(TZrUInt32 value, TZrUInt32 alignment) {
    TZrUInt32 remainder;

    if (alignment <= 1u) {
        return value;
    }
    remainder = value % alignment;
    return remainder == 0u ? value : value + (alignment - remainder);
}

static const TZrChar *ffi_contract_identifier_type_name(const SZrType *type) {
    if (type == ZR_NULL || type->name == ZR_NULL) {
        return ZR_NULL;
    }
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        type->name->data.identifier.name != ZR_NULL) {
        return ZrCore_String_GetNativeString(type->name->data.identifier.name);
    }
    if (type->name->type == ZR_AST_GENERIC_TYPE &&
        type->name->data.genericType.name != ZR_NULL &&
        type->name->data.genericType.name->name != ZR_NULL) {
        return ZrCore_String_GetNativeString(
                type->name->data.genericType.name->name);
    }
    return ZR_NULL;
}

static TZrBool ffi_contract_text_equals(
        const TZrChar *actual,
        const TZrChar *expected) {
    return (TZrBool)(actual != ZR_NULL && expected != ZR_NULL &&
                     strcmp(actual, expected) == 0);
}

static void ffi_contract_init_type(
        SZrFfiTypeContract *type,
        EZrFfiTypeKind kind,
        TZrUInt32 size,
        TZrUInt32 alignment,
        const TZrChar *typeName) {
    memset(type, 0, sizeof(*type));
    type->typeKind = kind;
    type->size = size;
    type->alignment = alignment;
    type->canonicalTypeHash = ffi_contract_hash_text(
            ZR_FFI_CONTRACT_FNV_OFFSET, typeName);
    if (kind != ZR_FFI_CONTRACT_TYPE_VOID) {
        type->flags = ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE;
    }
}

static EZrFfiContractStatus ffi_contract_init_primitive_type(
        const TZrChar *typeName,
        SZrFfiTypeContract *outType) {
    if (ffi_contract_text_equals(typeName, "void")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_VOID, 0u, 1u, typeName);
    } else if (ffi_contract_text_equals(typeName, "bool")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_BOOL, 1u, 1u, typeName);
    } else if (ffi_contract_text_equals(typeName, "i8")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_I8, 1u, 1u, typeName);
    } else if (ffi_contract_text_equals(typeName, "u8")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_U8, 1u, 1u, typeName);
    } else if (ffi_contract_text_equals(typeName, "i16")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_I16, 2u, 2u, typeName);
    } else if (ffi_contract_text_equals(typeName, "u16")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_U16, 2u, 2u, typeName);
    } else if (ffi_contract_text_equals(typeName, "i32") ||
               ffi_contract_text_equals(typeName, "int")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_I32, 4u, 4u, typeName);
    } else if (ffi_contract_text_equals(typeName, "u32")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_U32, 4u, 4u, typeName);
    } else if (ffi_contract_text_equals(typeName, "i64")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_I64, 8u, 8u, typeName);
    } else if (ffi_contract_text_equals(typeName, "u64")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_U64, 8u, 8u, typeName);
    } else if (ffi_contract_text_equals(typeName, "f32") ||
               ffi_contract_text_equals(typeName, "float32")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_F32, 4u, 4u, typeName);
    } else if (ffi_contract_text_equals(typeName, "f64") ||
               ffi_contract_text_equals(typeName, "float")) {
        ffi_contract_init_type(outType, ZR_FFI_CONTRACT_TYPE_F64, 8u, 8u, typeName);
    } else if (ffi_contract_text_equals(typeName, "usize")) {
        ffi_contract_init_type(
                outType,
                ZR_FFI_CONTRACT_TYPE_USIZE,
                (TZrUInt32)sizeof(TZrSize),
                (TZrUInt32)alignof(TZrSize),
                typeName);
    } else if (ffi_contract_text_equals(typeName, "isize")) {
        ffi_contract_init_type(
                outType,
                ZR_FFI_CONTRACT_TYPE_ISIZE,
                (TZrUInt32)sizeof(TZrMemoryOffset),
                (TZrUInt32)alignof(TZrMemoryOffset),
                typeName);
    } else if (ffi_contract_text_equals(typeName, "pointer") ||
               ffi_contract_text_equals(typeName, "Ptr")) {
        ffi_contract_init_type(
                outType,
                ZR_FFI_CONTRACT_TYPE_POINTER,
                (TZrUInt32)sizeof(TZrPtr),
                (TZrUInt32)alignof(TZrPtr),
                typeName);
    } else {
        return ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE;
    }
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_build_type(
        const SZrExternBlock *externBlock,
        const SZrType *syntaxType,
        SZrFfiTypeContract *outType,
        SZrFfiSignatureContract *signature,
        TZrUInt32 depth);

static EZrFfiContractStatus ffi_contract_abi_from_decorators(
        SZrAstNodeArray *decorators,
        EZrFfiAbi *outAbi) {
    SZrString *abiName = extern_compiler_decorators_get_string_arg(
            decorators, "callingConvention");
    const TZrChar *abiText;

    if (outAbi == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    if (abiName == ZR_NULL) {
        abiName = extern_compiler_decorators_get_string_arg(decorators, "callconv");
    }
    if (abiName == ZR_NULL) {
        *outAbi = ZR_FFI_CONTRACT_ABI_SYSTEM;
        return ZR_FFI_CONTRACT_STATUS_OK;
    }
    abiText = ZrCore_String_GetNativeString(abiName);
    if (ffi_contract_text_equals(abiText, "system")) {
        *outAbi = ZR_FFI_CONTRACT_ABI_SYSTEM;
    } else if (ffi_contract_text_equals(abiText, "c") ||
               ffi_contract_text_equals(abiText, "cdecl")) {
        *outAbi = ZR_FFI_CONTRACT_ABI_C;
    } else if (ffi_contract_text_equals(abiText, "stdcall")) {
        *outAbi = ZR_FFI_CONTRACT_ABI_STDCALL;
    } else {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ABI;
    }
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_parse_policies(
        SZrAstNodeArray *decorators,
        SZrFfiSignatureContract *signature) {
    SZrString *value;
    const TZrChar *text;

#define ZR_FFI_READ_POLICY(LEAF, FIELD, NONE_VALUE, CASES) \
    do { \
        value = extern_compiler_decorators_get_string_arg(decorators, LEAF); \
        if (value == ZR_NULL) { \
            signature->FIELD = NONE_VALUE; \
        } else { \
            text = ZrCore_String_GetNativeString(value); \
            CASES \
            else { return ZR_FFI_CONTRACT_STATUS_INVALID_POLICY; } \
        } \
    } while (0)

    ZR_FFI_READ_POLICY(
            "charset", charset, ZR_FFI_CONTRACT_CHARSET_NONE,
            if (ffi_contract_text_equals(text, "utf8")) {
                signature->charset = ZR_FFI_CONTRACT_CHARSET_UTF8;
            } else if (ffi_contract_text_equals(text, "utf16")) {
                signature->charset = ZR_FFI_CONTRACT_CHARSET_UTF16;
            } else if (ffi_contract_text_equals(text, "ansi")) {
                signature->charset = ZR_FFI_CONTRACT_CHARSET_ANSI;
            });
    ZR_FFI_READ_POLICY(
            "errorPolicy", errorPolicy, ZR_FFI_CONTRACT_ERROR_NONE,
            if (ffi_contract_text_equals(text, "returnCode")) {
                signature->errorPolicy = ZR_FFI_CONTRACT_ERROR_RETURN_CODE;
            } else if (ffi_contract_text_equals(text, "lastError")) {
                signature->errorPolicy = ZR_FFI_CONTRACT_ERROR_LAST_ERROR;
            } else if (ffi_contract_text_equals(text, "errno")) {
                signature->errorPolicy = ZR_FFI_CONTRACT_ERROR_ERRNO;
            } else if (ffi_contract_text_equals(text, "throws")) {
                signature->errorPolicy = ZR_FFI_CONTRACT_ERROR_THROWS;
            });
    ZR_FFI_READ_POLICY(
            "cleanup", cleanupPolicy, ZR_FFI_CONTRACT_CLEANUP_NONE,
            if (ffi_contract_text_equals(text, "caller")) {
                signature->cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_CALLER;
            } else if (ffi_contract_text_equals(text, "callee")) {
                signature->cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_CALLEE;
            } else if (ffi_contract_text_equals(text, "registered")) {
                signature->cleanupPolicy = ZR_FFI_CONTRACT_CLEANUP_REGISTERED;
            });
    ZR_FFI_READ_POLICY(
            "callbackLifetime", callbackLifetime,
            ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE,
            if (ffi_contract_text_equals(text, "call")) {
                signature->callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_CALL;
            } else if (ffi_contract_text_equals(text, "scoped")) {
                signature->callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_SCOPED;
            } else if (ffi_contract_text_equals(text, "static")) {
                signature->callbackLifetime = ZR_FFI_CONTRACT_CALLBACK_LIFETIME_STATIC;
            });
    ZR_FFI_READ_POLICY(
            "callbackThread", callbackThreadPolicy,
            ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE,
            if (ffi_contract_text_equals(text, "caller")) {
                signature->callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_CALLER;
            } else if (ffi_contract_text_equals(text, "attach")) {
                signature->callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_ATTACH;
            } else if (ffi_contract_text_equals(text, "forbidden")) {
                signature->callbackThreadPolicy = ZR_FFI_CONTRACT_CALLBACK_THREAD_FORBIDDEN;
            });
    ZR_FFI_READ_POLICY(
            "callbackException", callbackExceptionPolicy,
            ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE,
            if (ffi_contract_text_equals(text, "abort")) {
                signature->callbackExceptionPolicy = ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ABORT;
            } else if (ffi_contract_text_equals(text, "returnDefault")) {
                signature->callbackExceptionPolicy =
                        ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_RETURN_DEFAULT;
            } else if (ffi_contract_text_equals(text, "errorResult")) {
                signature->callbackExceptionPolicy =
                        ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_ERROR_RESULT;
            });

#undef ZR_FFI_READ_POLICY
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiDirection ffi_contract_direction_from_parameter(
        const SZrParameter *parameter) {
    if (parameter == ZR_NULL) {
        return ZR_FFI_CONTRACT_DIRECTION_IN;
    }
    switch (parameter->passingMode) {
        case ZR_PARAMETER_PASSING_MODE_REF:
            return ZR_FFI_CONTRACT_DIRECTION_REF;
        case ZR_PARAMETER_PASSING_MODE_OUT:
            return ZR_FFI_CONTRACT_DIRECTION_OUT;
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        case ZR_PARAMETER_PASSING_MODE_IN:
        default:
            return ZR_FFI_CONTRACT_DIRECTION_IN;
    }
}

static EZrFfiContractStatus ffi_contract_build_signature(
        const SZrExternBlock *externBlock,
        SZrAstNodeArray *parameters,
        const SZrParameter *variadicParameter,
        const SZrType *returnType,
        SZrAstNodeArray *decorators,
        SZrFfiSignatureContract *outSignature,
        SZrFfiContractDiagnostic *diagnostic,
        SZrFileRange sourceRange,
        TZrUInt32 depth) {
    EZrFfiContractStatus status;
    TZrBool hasCallback = ZR_FALSE;

    if (outSignature == ZR_NULL || depth > ZR_FFI_CONTRACT_MAX_TYPE_DEPTH) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    memset(outSignature, 0, sizeof(*outSignature));
    status = ffi_contract_abi_from_decorators(decorators, &outSignature->abi);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    outSignature->targetPointerSize = (TZrUInt32)sizeof(TZrPtr);
    outSignature->targetEndianness = ZR_IO_IS_LITTLE_ENDIAN
                                             ? ZR_FFI_CONTRACT_ENDIAN_LITTLE
                                             : ZR_FFI_CONTRACT_ENDIAN_BIG;
    strncpy(
            outSignature->targetTriple,
            ZrCommon_FfiContract_GetHostTargetTriple(),
            sizeof(outSignature->targetTriple) - 1u);
    outSignature->targetAbiHash = ZrCommon_FfiContract_ComputeTargetAbiHash(
            outSignature->abi,
            outSignature->targetPointerSize,
            outSignature->targetEndianness,
            outSignature->targetTriple);
    status = ffi_contract_parse_policies(decorators, outSignature);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    outSignature->isVariadic = variadicParameter != ZR_NULL;
    outSignature->parameterCount = parameters != ZR_NULL
                                           ? (TZrUInt32)parameters->count
                                           : 0u;
    if (outSignature->parameterCount > ZR_FFI_CONTRACT_MAX_PARAMETERS) {
        ffi_contract_set_diagnostic(
                diagnostic,
                ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT,
                outSignature->parameterCount,
                sourceRange);
        return ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT;
    }

    if (returnType == ZR_NULL) {
        ffi_contract_init_type(
                &outSignature->returnType,
                ZR_FFI_CONTRACT_TYPE_VOID,
                0u,
                1u,
                "void");
    } else {
        status = ffi_contract_build_type(
                externBlock,
                returnType,
                &outSignature->returnType,
                outSignature,
                depth + 1u);
        if (status != ZR_FFI_CONTRACT_STATUS_OK) {
            ffi_contract_set_diagnostic(diagnostic, status, UINT32_MAX, sourceRange);
            return status;
        }
        if (outSignature->returnType.typeKind ==
                ZR_FFI_CONTRACT_TYPE_UNION) {
            ffi_contract_set_diagnostic(
                    diagnostic,
                    ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE,
                    UINT32_MAX,
                    sourceRange);
            return ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE;
        }
    }
    if (outSignature->errorPolicy == ZR_FFI_CONTRACT_ERROR_THROWS ||
        outSignature->cleanupPolicy == ZR_FFI_CONTRACT_CLEANUP_REGISTERED ||
        (outSignature->errorPolicy == ZR_FFI_CONTRACT_ERROR_RETURN_CODE &&
         !ZrCommon_FfiReturnCodeType_Validate(&outSignature->returnType))) {
        ffi_contract_set_diagnostic(
                diagnostic,
                ZR_FFI_CONTRACT_STATUS_INVALID_POLICY,
                UINT32_MAX,
                sourceRange);
        return ZR_FFI_CONTRACT_STATUS_INVALID_POLICY;
    }

    for (TZrUInt32 index = 0u; index < outSignature->parameterCount; index++) {
        const SZrAstNode *parameterNode = parameters->nodes[index];
        const SZrParameter *parameter;
        SZrFfiParameterContract *parameterContract = &outSignature->parameters[index];

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
        }
        parameter = &parameterNode->data.parameter;
        status = ffi_contract_build_type(
                externBlock,
                parameter->typeInfo,
                &parameterContract->type,
                outSignature,
                depth + 1u);
        if (status != ZR_FFI_CONTRACT_STATUS_OK) {
            ffi_contract_set_diagnostic(
                    diagnostic, status, index, parameterNode->location);
            return status;
        }
        parameterContract->direction = ffi_contract_direction_from_parameter(parameter);
        if (parameterContract->type.typeKind == ZR_FFI_CONTRACT_TYPE_UNION &&
            parameterContract->direction != ZR_FFI_CONTRACT_DIRECTION_IN) {
            ffi_contract_set_diagnostic(
                    diagnostic,
                    ZR_FFI_CONTRACT_STATUS_INVALID_DIRECTION,
                    index,
                    parameterNode->location);
            return ZR_FFI_CONTRACT_STATUS_INVALID_DIRECTION;
        }
        parameterContract->marshalling = ZR_FFI_CONTRACT_MARSHALLING_DIRECT;
        parameterContract->ownership = ZR_FFI_CONTRACT_OWNERSHIP_BORROWED;
        if (parameterContract->type.typeKind == ZR_FFI_CONTRACT_TYPE_CALLBACK) {
            hasCallback = ZR_TRUE;
        }
    }
    if (hasCallback &&
        (outSignature->callbackLifetime == ZR_FFI_CONTRACT_CALLBACK_LIFETIME_NONE ||
         outSignature->callbackThreadPolicy == ZR_FFI_CONTRACT_CALLBACK_THREAD_NONE ||
         outSignature->callbackExceptionPolicy ==
                 ZR_FFI_CONTRACT_CALLBACK_EXCEPTION_NONE)) {
        return ZR_FFI_CONTRACT_STATUS_CALLBACK_POLICY_REQUIRED;
    }
    outSignature->signatureHash =
            ZrCommon_FfiSignatureContract_ComputeHash(outSignature);
    return outSignature->signatureHash != 0u
                   ? ZR_FFI_CONTRACT_STATUS_OK
                   : ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
}

EZrFfiContractStatus ZrParser_FfiContract_BuildDelegateSignature(
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiSignatureContract *outSignature,
        SZrFfiContractDiagnostic *diagnostic) {
    const SZrExternDelegateDeclaration *delegate;
    EZrFfiContractStatus status;

    if (externBlock == ZR_NULL || declaration == ZR_NULL ||
        declaration->type != ZR_AST_EXTERN_DELEGATE_DECLARATION ||
        outSignature == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    delegate = &declaration->data.externDelegateDeclaration;
    status = ffi_contract_build_signature(
            externBlock,
            delegate->params,
            delegate->args,
            delegate->returnType,
            delegate->decorators,
            outSignature,
            diagnostic,
            declaration->location,
            0u);
    ffi_contract_set_diagnostic(
            diagnostic, status, UINT32_MAX, declaration->location);
    return status;
}

static EZrFfiContractStatus ffi_contract_build_aggregate_type(
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiTypeContract *outType,
        SZrFfiSignatureContract *signature,
        TZrUInt32 depth) {
    const SZrStructDeclaration *structure = &declaration->data.structDeclaration;
    const TZrChar *typeName;
    SZrString *kindValue;
    const TZrChar *kindText;
    TZrBool isUnion = ZR_FALSE;
    TZrUInt32 currentSize = 0u;
    TZrUInt32 maxAlignment = 1u;
    TZrInt64 packValue = 0;
    TZrInt64 explicitAlignment = 0;
    TZrUInt64 layoutHash = ZR_FFI_CONTRACT_FNV_OFFSET;
    TZrUInt32 aggregateFieldCount = 0u;
    TZrUInt32 aggregateFieldStart;

    if (structure->isRefLike || signature == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE;
    }
    aggregateFieldStart = signature->aggregateFieldCount;
    typeName = structure->name != ZR_NULL && structure->name->name != ZR_NULL
                       ? ZrCore_String_GetNativeString(structure->name->name)
                       : ZR_NULL;
    if (typeName == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    kindValue = extern_compiler_decorators_get_string_arg(structure->decorators, "kind");
    kindText = kindValue != ZR_NULL ? ZrCore_String_GetNativeString(kindValue) : ZR_NULL;
    if (kindText != ZR_NULL) {
        if (ffi_contract_text_equals(kindText, "union")) {
            isUnion = ZR_TRUE;
        } else if (!ffi_contract_text_equals(kindText, "struct")) {
            return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
        }
    }
    if (extern_compiler_decorators_get_int_arg(
                structure->decorators, "pack", &packValue) &&
        (packValue <= 0 || packValue > UINT32_MAX ||
         (((TZrUInt32)packValue & ((TZrUInt32)packValue - 1u)) != 0u))) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    if (extern_compiler_decorators_get_int_arg(
                structure->decorators, "align", &explicitAlignment) &&
        (explicitAlignment <= 0 || explicitAlignment > UINT32_MAX ||
         (((TZrUInt32)explicitAlignment &
           ((TZrUInt32)explicitAlignment - 1u)) != 0u))) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }

    layoutHash = ZrCommon_FfiContract_HashU32(
            layoutHash,
            isUnion ? (TZrUInt32)ZR_FFI_CONTRACT_TYPE_UNION
                    : (TZrUInt32)ZR_FFI_CONTRACT_TYPE_STRUCT);
    if (structure->members != ZR_NULL) {
        for (TZrSize index = 0u; index < structure->members->count; index++) {
            const SZrAstNode *member = structure->members->nodes[index];
            SZrFfiTypeContract fieldType;
            TZrUInt32 fieldAlignment;
            TZrUInt32 fieldOffset;
            TZrUInt32 fieldEnd;
            TZrInt64 explicitOffset = -1;
            EZrFfiContractStatus status;

            if (member == ZR_NULL || member->type != ZR_AST_STRUCT_FIELD ||
                member->data.structField.isStatic) {
                continue;
            }
            status = ffi_contract_build_type(
                    externBlock,
                    member->data.structField.typeInfo,
                    &fieldType,
                    signature,
                    depth + 1u);
            if (status != ZR_FFI_CONTRACT_STATUS_OK) {
                return status;
            }
            if (fieldType.typeKind == ZR_FFI_CONTRACT_TYPE_VOID ||
                fieldType.typeKind == ZR_FFI_CONTRACT_TYPE_STRUCT ||
                fieldType.typeKind == ZR_FFI_CONTRACT_TYPE_UNION ||
                (fieldType.flags & ZR_FFI_CONTRACT_TYPE_FLAG_BLITTABLE) == 0u) {
                return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
            }
            if (signature->aggregateFieldCount >=
                        ZR_FFI_CONTRACT_MAX_AGGREGATE_FIELDS ||
                member->data.structField.name == ZR_NULL ||
                member->data.structField.name->name == ZR_NULL) {
                return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
            }
            fieldAlignment = fieldType.alignment;
            if (packValue > 0 && fieldAlignment > (TZrUInt32)packValue) {
                fieldAlignment = (TZrUInt32)packValue;
            }
            if (extern_compiler_decorators_get_int_arg(
                        member->data.structField.decorators,
                        "offset",
                        &explicitOffset)) {
                if (explicitOffset < 0 || explicitOffset > UINT32_MAX) {
                    return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
                }
                fieldOffset = (TZrUInt32)explicitOffset;
            } else if (isUnion) {
                fieldOffset = 0u;
            } else {
                fieldOffset = ffi_contract_align_up(currentSize, fieldAlignment);
            }
            if (fieldOffset > UINT32_MAX - fieldType.size) {
                return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
            }
            fieldEnd = fieldOffset + fieldType.size;
            if (fieldEnd > currentSize) {
                currentSize = fieldEnd;
            }
            if (fieldAlignment > maxAlignment) {
                maxAlignment = fieldAlignment;
            }
            {
                SZrFfiAggregateFieldContract *field =
                        &signature->aggregateFields[
                                signature->aggregateFieldCount];
                const TZrChar *fieldName = ZrCore_String_GetNativeString(
                        member->data.structField.name->name);

                if (ffi_contract_copy_text(
                            field->name,
                            ZR_FFI_CONTRACT_FIELD_NAME_CAPACITY,
                            fieldName,
                            ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT) !=
                    ZR_FFI_CONTRACT_STATUS_OK) {
                    return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
                }
                field->typeKind = fieldType.typeKind;
                field->size = fieldType.size;
                field->alignment = fieldType.alignment;
                field->offset = fieldOffset;
                signature->aggregateFieldCount++;
                aggregateFieldCount++;
            }
            layoutHash = ZrCommon_FfiContract_HashU32(layoutHash, fieldOffset);
            layoutHash = ZrCommon_FfiContract_HashType(layoutHash, &fieldType);
        }
    }
    if (explicitAlignment > 0 && maxAlignment < (TZrUInt32)explicitAlignment) {
        maxAlignment = (TZrUInt32)explicitAlignment;
    }
    currentSize = ffi_contract_align_up(currentSize, maxAlignment);
    if (currentSize == 0u) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    layoutHash = ZrCommon_FfiContract_HashU32(layoutHash, currentSize);
    layoutHash = ZrCommon_FfiContract_HashU32(layoutHash, maxAlignment);
    ffi_contract_init_type(
            outType,
            isUnion ? ZR_FFI_CONTRACT_TYPE_UNION : ZR_FFI_CONTRACT_TYPE_STRUCT,
            currentSize,
            maxAlignment,
            typeName);
    outType->layoutHash = layoutHash;
    outType->aggregateFieldStart = aggregateFieldStart;
    outType->aggregateFieldCount = aggregateFieldCount;
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_build_enum_type(
        const SZrAstNode *declaration,
        SZrFfiTypeContract *outType) {
    const SZrEnumDeclaration *enumeration = &declaration->data.enumDeclaration;
    SZrFfiTypeContract underlying;
    const TZrChar *typeName;
    const TZrChar *underlyingName = "i32";
    EZrFfiContractStatus status;

    if (enumeration->baseType != ZR_NULL) {
        underlyingName = ffi_contract_identifier_type_name(enumeration->baseType);
    }
    status = ffi_contract_init_primitive_type(underlyingName, &underlying);
    if (status != ZR_FFI_CONTRACT_STATUS_OK ||
        underlying.typeKind == ZR_FFI_CONTRACT_TYPE_VOID ||
        underlying.typeKind >= ZR_FFI_CONTRACT_TYPE_F32) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    typeName = enumeration->name != ZR_NULL && enumeration->name->name != ZR_NULL
                       ? ZrCore_String_GetNativeString(enumeration->name->name)
                       : ZR_NULL;
    if (typeName == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    ffi_contract_init_type(
            outType,
            ZR_FFI_CONTRACT_TYPE_ENUM,
            underlying.size,
            underlying.alignment,
            typeName);
    outType->layoutHash = ZrCommon_FfiContract_HashType(
            ZR_FFI_CONTRACT_FNV_OFFSET, &underlying);
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_build_callback_type(
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrFfiTypeContract *outType,
        TZrUInt32 depth) {
    const SZrExternDelegateDeclaration *delegate =
            &declaration->data.externDelegateDeclaration;
    SZrFfiSignatureContract signature;
    const TZrChar *typeName;
    EZrFfiContractStatus status;

    status = ffi_contract_build_signature(
            externBlock,
            delegate->params,
            delegate->args,
            delegate->returnType,
            delegate->decorators,
            &signature,
            ZR_NULL,
            declaration->location,
            depth + 1u);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    typeName = delegate->name != ZR_NULL && delegate->name->name != ZR_NULL
                       ? ZrCore_String_GetNativeString(delegate->name->name)
                       : ZR_NULL;
    if (typeName == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_LAYOUT;
    }
    ffi_contract_init_type(
            outType,
            ZR_FFI_CONTRACT_TYPE_CALLBACK,
            (TZrUInt32)sizeof(TZrPtr),
            (TZrUInt32)alignof(TZrPtr),
            typeName);
    outType->layoutHash = signature.signatureHash;
    return ZR_FFI_CONTRACT_STATUS_OK;
}

static EZrFfiContractStatus ffi_contract_build_type(
        const SZrExternBlock *externBlock,
        const SZrType *syntaxType,
        SZrFfiTypeContract *outType,
        SZrFfiSignatureContract *signature,
        TZrUInt32 depth) {
    const TZrChar *typeName = ffi_contract_identifier_type_name(syntaxType);
    EZrFfiContractStatus status;

    if (syntaxType == ZR_NULL || outType == ZR_NULL || typeName == ZR_NULL ||
        depth > ZR_FFI_CONTRACT_MAX_TYPE_DEPTH) {
        return ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE;
    }
    if (syntaxType->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        syntaxType->dimensions != 0 || syntaxType->isScopedReference ||
        syntaxType->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        syntaxType->isReadonlyView) {
        return ZR_FFI_CONTRACT_STATUS_FORBIDDEN_MANAGED_TYPE;
    }
    if (syntaxType->name->type == ZR_AST_GENERIC_TYPE) {
        const SZrGenericType *generic = &syntaxType->name->data.genericType;

        if ((ffi_contract_text_equals(typeName, "pointer") ||
             ffi_contract_text_equals(typeName, "Ptr")) &&
            generic->params != ZR_NULL && generic->params->count == 1u &&
            generic->params->nodes[0] != ZR_NULL &&
            generic->params->nodes[0]->type == ZR_AST_TYPE) {
            SZrFfiTypeContract pointee;

            status = ffi_contract_build_type(
                    externBlock,
                    &generic->params->nodes[0]->data.type,
                    &pointee,
                    signature,
                    depth + 1u);
            if (status != ZR_FFI_CONTRACT_STATUS_OK) {
                return status;
            }
            ffi_contract_init_type(
                    outType,
                    ZR_FFI_CONTRACT_TYPE_POINTER,
                    (TZrUInt32)sizeof(TZrPtr),
                    (TZrUInt32)alignof(TZrPtr),
                    typeName);
            outType->canonicalTypeHash = ZrCommon_FfiContract_HashU64(
                    outType->canonicalTypeHash, pointee.canonicalTypeHash);
            outType->layoutHash = ZrCommon_FfiContract_HashType(
                    ZR_FFI_CONTRACT_FNV_OFFSET, &pointee);
            return ZR_FFI_CONTRACT_STATUS_OK;
        }
        return ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE;
    }

    status = ffi_contract_init_primitive_type(typeName, outType);
    if (status == ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    if (externBlock != ZR_NULL && syntaxType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *name = syntaxType->name->data.identifier.name;
        SZrAstNode *declaration = extern_compiler_find_named_declaration(
                (SZrExternBlock *)externBlock, name);

        if (declaration != ZR_NULL) {
            switch (declaration->type) {
                case ZR_AST_STRUCT_DECLARATION:
                    return ffi_contract_build_aggregate_type(
                            externBlock,
                            declaration,
                            outType,
                            signature,
                            depth + 1u);
                case ZR_AST_ENUM_DECLARATION:
                    return ffi_contract_build_enum_type(declaration, outType);
                case ZR_AST_EXTERN_DELEGATE_DECLARATION:
                    return ffi_contract_build_callback_type(
                            externBlock, declaration, outType, depth + 1u);
                default:
                    break;
            }
        }
    }
    memset(outType, 0, sizeof(*outType));
    outType->typeKind = ZR_FFI_CONTRACT_TYPE_UNSUPPORTED;
    outType->canonicalTypeHash = ffi_contract_hash_text(
            ZR_FFI_CONTRACT_FNV_OFFSET, typeName);
    return ZR_FFI_CONTRACT_STATUS_UNSUPPORTED_TYPE;
}

EZrFfiContractStatus ZrParser_FfiContract_Build(
        SZrSemanticContext *semanticContext,
        const SZrExternBlock *externBlock,
        const SZrAstNode *declaration,
        SZrNativeImportContract *outContract,
        SZrFfiContractDiagnostic *diagnostic) {
    const SZrExternFunctionDeclaration *function;
    const TZrChar *libraryText;
    const TZrChar *entryText;
    SZrString *entryOverride;
    EZrFfiContractStatus status;

    if (semanticContext == ZR_NULL || externBlock == ZR_NULL || declaration == ZR_NULL ||
        outContract == ZR_NULL ||
        declaration->type != ZR_AST_EXTERN_FUNCTION_DECLARATION ||
        externBlock->libraryName == ZR_NULL ||
        externBlock->libraryName->type != ZR_AST_STRING_LITERAL ||
        externBlock->libraryName->data.stringLiteral.value == ZR_NULL) {
        ffi_contract_set_diagnostic(
                diagnostic,
                ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT,
                0u,
                declaration != ZR_NULL ? declaration->location : (SZrFileRange){0});
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }

    memset(outContract, 0, sizeof(*outContract));
    function = &declaration->data.externFunctionDeclaration;
    if (function->name == ZR_NULL || function->name->name == ZR_NULL) {
        return ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    }
    libraryText = ZrCore_String_GetNativeString(
            externBlock->libraryName->data.stringLiteral.value);
    entryText = ZrCore_String_GetNativeString(function->name->name);
    entryOverride = extern_compiler_decorators_get_string_arg(
            function->decorators, "entry");
    if (entryOverride != ZR_NULL) {
        entryText = ZrCore_String_GetNativeString(entryOverride);
    }

    outContract->schemaVersion = ZR_FFI_CONTRACT_SCHEMA_VERSION;
    status = ffi_contract_copy_text(
            outContract->libraryLocator,
            ZR_FFI_CONTRACT_LIBRARY_CAPACITY,
            libraryText,
            ZR_FFI_CONTRACT_STATUS_LIBRARY_LOCATOR_LIMIT);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    status = ffi_contract_copy_text(
            outContract->entryPoint,
            ZR_FFI_CONTRACT_ENTRY_CAPACITY,
            entryText,
            ZR_FFI_CONTRACT_STATUS_ENTRY_POINT_LIMIT);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        return status;
    }
    status = ffi_contract_build_signature(
            externBlock,
            function->params,
            function->args,
            function->returnType,
            function->decorators,
            &outContract->signature,
            diagnostic,
            declaration->location,
            0u);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        ffi_contract_set_diagnostic(
                diagnostic, status, diagnostic != ZR_NULL
                                            ? diagnostic->parameterIndex
                                            : 0u,
                declaration->location);
        return status;
    }

    status = compiler_ffi_callable_contract_build(
            semanticContext,
            externBlock,
            declaration,
            &outContract->signature,
            &outContract->callable);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        ffi_contract_set_diagnostic(
                diagnostic, status, 0u, declaration->location);
        return status;
    }
    outContract->symbolId = ffi_contract_hash_text(
            ffi_contract_hash_text(
                    ZR_FFI_CONTRACT_FNV_OFFSET, libraryText),
            entryText);
    status = ffi_contract_build_import_metadata(
            function, declaration->location, outContract);
    if (status != ZR_FFI_CONTRACT_STATUS_OK) {
        ffi_contract_set_diagnostic(
                diagnostic, status, 0u, declaration->location);
        return status;
    }
    ffi_contract_set_diagnostic(
            diagnostic,
            ZR_FFI_CONTRACT_STATUS_OK,
            0u,
            declaration->location);
    return ZR_FFI_CONTRACT_STATUS_OK;
}

EZrFfiContractStatus ZrParser_FfiContract_Validate(
        const SZrNativeImportContract *contract,
        SZrFfiContractDiagnostic *diagnostic) {
    EZrFfiContractStatus status = ZR_FFI_CONTRACT_STATUS_OK;

    if (contract == ZR_NULL) {
        status = ZR_FFI_CONTRACT_STATUS_INVALID_ARGUMENT;
    } else if (contract->schemaVersion != ZR_FFI_CONTRACT_SCHEMA_VERSION) {
        status = ZR_FFI_CONTRACT_STATUS_SCHEMA_MISMATCH;
    } else if (memchr(
                       contract->libraryLocator,
                       '\0',
                       sizeof(contract->libraryLocator)) == ZR_NULL) {
        status = ZR_FFI_CONTRACT_STATUS_LIBRARY_LOCATOR_LIMIT;
    } else if (memchr(
                       contract->entryPoint,
                       '\0',
                       sizeof(contract->entryPoint)) == ZR_NULL) {
        status = ZR_FFI_CONTRACT_STATUS_ENTRY_POINT_LIMIT;
    } else if (contract->signature.parameterCount >
               ZR_FFI_CONTRACT_MAX_PARAMETERS) {
        status = ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT;
    } else if (contract->signature.abi > ZR_FFI_CONTRACT_ABI_STDCALL) {
        status = ZR_FFI_CONTRACT_STATUS_INVALID_ABI;
    } else if (contract->signature.targetAbiHash !=
               ZrCommon_FfiContract_ComputeTargetAbiHash(
                       contract->signature.abi,
                       contract->signature.targetPointerSize,
                       contract->signature.targetEndianness,
                       contract->signature.targetTriple)) {
        status = ZR_FFI_CONTRACT_STATUS_INVALID_TARGET_ABI;
    } else if (!ZrCommon_NativeImportContract_Validate(contract)) {
        status = ZR_FFI_CONTRACT_STATUS_HASH_MISMATCH;
    }
    ffi_contract_set_diagnostic(
            diagnostic,
            status,
            status == ZR_FFI_CONTRACT_STATUS_PARAMETER_LIMIT
                    ? contract->signature.parameterCount
                    : 0u,
            (SZrFileRange){0});
    return status;
}
